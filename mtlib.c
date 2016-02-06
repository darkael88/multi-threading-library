/*
Copyright : Darty Michael, 19/11/2014

mdarty.mtlib@gmail.com

This software is a computer program whose purpose is to facilitating the 
development of multithreaded program using.

This software is governed by the CeCILL license under French law and
abiding by the rules of distribution of free software.  You can  use, 
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info". 

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability. 

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or 
data to be ensured and,  more generally, to use and operate it in the 
same conditions as regards security. 

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.
*/


/*
************************data_file******************
*/

#include <mtlib.h>

# define 		NB_FCT_FOR_THREAD 		100
# define 		NB_JOIN_MAX				100
# define 		NB_THREAD_STD 			-1
# define 		THREAD_EN_CREATION		1
# define 		THREAD_TO_DESTROY		-1

# define 		STD						-1

char 			*mtl_no_ret;
int 			mtl_no_status;

# define 		NO_STATUS				&mtl_no_status
# define 		NO_RET 					mtl_no_ret

# define 		SUCCESS_FUNCTION		1

typedef struct 			s_mtl_fct
{
	int 				*status;
	int 				type_ret;
	void		 		*dest;
	void 				*(*fct)();
	int 				size_ret;
	size_t 				nb_arg;
	size_t 				argv[10];
	struct s_mtl_fct 	*next;
}						mtl_fct;

//contient les infos spécifique à chaque thread et sa liste d'attente propre (envoyé depuis le main)
typedef struct 			s_mtl_thread
{
	pthread_t 			thread;
	unsigned int 		num_thread;//ce numero ne change pas, meme en cas de destruction de thread
	pthread_mutex_t		thread_mutex;
	mtl_fct 			*dedicated_list;//ne peut être rempli que par le main. Cette liste est prioritaire au autre. le main envoi egalement les ordre de destruction individuelle.
	mtl_fct 			*end_dedicated_list;
}						mtl_thread;

//contient les infos partagé entre les thread comme les liste de fonction a utilisé/free, le nombre de thread ... etc etc
typedef struct 			s_mtl_pool
{
	int 				status;//0 == normal, 1 == thread en creation, 2 == thread en destruction. il faut attendre dans ces cas.3 == add dedicated ce qui empechera le thread de quitter entre temps.
	unsigned int 		nb_thread_to_destroy;//le nombre de thread devant être détruit et en cours
	unsigned int 		size_tab_thread;//size du tableau contenant les thread
	unsigned int 		nb_thread;//nombre de thread réel.
	mtl_thread 			**tab_thread;//tableau de pointeur contenant les structures des threads.
	mtl_fct 			*list_free;//list des fonctions 
	mtl_fct 			*list_fct;
	mtl_fct 			*end_list_fct;//permet de mettre les éléments directement en fin de liste.
	pthread_mutex_t 	pool_mutex;
	pthread_cond_t 		pool_cond;
}						mtl_pool;

//structure contenant les données du main, la liste de fonction a executer/repartir coté main et les données general
typedef struct 			s_mtl_glob
{
	long 				nb_thread_hard;//recuperer par sysconf
	pthread_mutex_t 	glob_mutex;
	mtl_join 			*list_join;
	unsigned int 		nb_join;
	mtl_pool 			*pool;//pointe sur l'endroit où est enregistrer les infos du pool
}						mtl_glob;

typedef enum 			sigmtl
{
	MTL,
}						sigmtl;


static void			st_memdel(void **data)
{
	free(*data);
	*data = NULL;
}


/*
*********stock info global********
*/

static mtl_glob			*st_sig_mtl(void)
{
	static mtl_glob		mtl;

	return (&mtl);
}


static void 			mtl_push_front_list_free(mtl_pool *pool, mtl_fct *src)
{
	pthread_mutex_lock(&pool->pool_mutex);
	src->next = pool->list_free;
	pool->list_free = src;
	pthread_cond_broadcast(&pool->pool_cond);//un seul peut en beneficier, sert a rien de tous els rveiller
	pthread_mutex_unlock(&pool->pool_mutex);
}

static void			*st_exec_function(mtl_fct *fct)
{
	size_t			nb_arg;
	size_t			*argv;
	void			*value;

	argv = (size_t *)fct->argv;
	nb_arg = fct->nb_arg;
	if (nb_arg == 0)
		value = fct->fct();
	else if (nb_arg == 1)
		value = fct->fct(argv[0]);
	else if (nb_arg == 2)
		value = fct->fct(argv[0], argv[1]);
	else if (nb_arg == 3)
		value = fct->fct(argv[0], argv[1], argv[2]);
	else if (nb_arg == 4)
		value = fct->fct(argv[0], argv[1], argv[2], argv[3]);
	else if (nb_arg == 5)
		value = fct->fct(argv[0], argv[1], argv[2], argv[3], argv[4]);
	else if (nb_arg == 6)
		value = fct->fct(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
	else if (nb_arg == 7)
		value = fct->fct(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
	else if (nb_arg == 8)
		value = fct->fct(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
	else if (nb_arg == 9)
		value = fct->fct(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
	else if (nb_arg == 10)
		value = fct->fct(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9]);
	else
		return (0);
	return (value);
}

static void 	mtl_check_to_exec_list(mtl_pool *pool, mtl_fct *list)
{
	void		*value;

	value = st_exec_function(list);
	if (list->type_ret == PTR)
		*(void **)list->dest = value;
	else
		memcpy(list->dest, &value, list->size_ret);
	*(list->status) = SUCCESS_FUNCTION;
	list->next = NULL;
	mtl_push_front_list_free(pool, list);
}


static void 	st_thread_check_dedicated_list(mtl_pool *pool, mtl_thread *cur)
{
	mtl_fct 	*list;
	mtl_fct 	*tmp;

	while (1)
	{
		pthread_mutex_lock(&cur->thread_mutex);
		list = cur->dedicated_list;
		cur->dedicated_list = NULL;
		cur->end_dedicated_list = NULL;
		pthread_mutex_unlock(&cur->thread_mutex);
		if (list)
		{
			while (list)
			{
				tmp = list->next;
				mtl_check_to_exec_list(pool, list);
				list = tmp;
			}
		}
		else
			break ;//rien en dédié
	}
}

static int		mtl_check_share_list(mtl_pool *pool)
{
	mtl_fct 	*tmp;

	pthread_mutex_lock(&pool->pool_mutex);
	tmp = pool->list_fct;
	if (tmp)
	{
		pool->list_fct = pool->list_fct->next;
		if (pool->list_fct == NULL)
			pool->end_list_fct = NULL;
		pthread_mutex_unlock(&pool->pool_mutex);
		mtl_check_to_exec_list(pool, tmp);
		return (1);
	}
	else
	{
		pthread_mutex_unlock(&pool->pool_mutex);
		return (0);
	}
}

static void		*mtl_action_thread(void *data)
{
	mtl_thread 	*current;
	mtl_pool 	*pool;

	current = (mtl_thread *)data;
	pool = (st_sig_mtl())->pool;
	//indiquer que le thread est créé
	pthread_mutex_lock(&pool->pool_mutex);
	pool->nb_thread++;
	pthread_cond_broadcast(&pool->pool_cond);
	pthread_mutex_unlock(&pool->pool_mutex);
	while (1)
	{
		st_thread_check_dedicated_list(pool, current);
		mtl_check_share_list(pool);
		if (pool->status == THREAD_TO_DESTROY)
		{
			while (mtl_check_share_list(pool))
				;
			st_thread_check_dedicated_list(pool, current);
			break ;
		}
	}
	return (NULL);
}



static void 			mtl_push_back_mtl_fct(mtl_fct **dest, mtl_fct **end, mtl_fct *src)
{
	if (*end)
		(*end)->next = src;
	else
		*dest = src;
	*end = src;
}

static mtl_fct			*mtl_search_block_free(mtl_pool *pool)
{
	mtl_fct				*tmp;
	
	while (1)
	{
		pthread_mutex_lock(&pool->pool_mutex);
		if (pool->list_free)
		{
			tmp = pool->list_free;
			pool->list_free = pool->list_free->next;
			tmp->next = NULL;
			pthread_mutex_unlock(&pool->pool_mutex);
			return (tmp);
		}
		else
		{
			pthread_mutex_unlock(&pool->pool_mutex);
			if (mtl_exec_one_share_function() == 0)
				pthread_cond_wait(&pool->pool_cond, &pool->pool_mutex);
		}

	}
}

static int 		mtl_thread_check_dedicated_fct(mtl_pool *pool, mtl_thread *cur)
{
	mtl_fct 	*list;

	pthread_mutex_lock(&cur->thread_mutex);
	//je prends un seul élément
	list = cur->dedicated_list;
	if (list)
	{
		cur->dedicated_list = list->next;
		if (cur->dedicated_list == NULL)
			cur->end_dedicated_list = NULL;
		pthread_mutex_unlock(&cur->thread_mutex);
		mtl_check_to_exec_list(pool, list);
		return (1);
	}
	else
	{
		pthread_mutex_unlock(&cur->thread_mutex);
		return (0) ;//rien en dédié
	}
}

/*
****exec function
*/






static mtl_thread*	create_mtl_thread(const unsigned int num)
{
	mtl_thread 		*elem;

	elem = malloc(sizeof(mtl_thread));
	if (elem == NULL)
		return (NULL);

	elem->num_thread = num;
	elem->dedicated_list = NULL;
	elem->end_dedicated_list = NULL;
	if (pthread_mutex_init(&elem->thread_mutex, NULL))
	{
		st_memdel((void **)&elem);
		return (NULL);
	}
	return (elem);
}

static void 		st_check_create_thread(const unsigned int nb_create, 
										mtl_pool *pool)
{
	while (1)
	{
		pthread_mutex_lock(&pool->pool_mutex);
		if (pool->nb_thread == nb_create)
		{
			pool->status = 0;
			pthread_cond_broadcast(&pool->pool_cond);//a 
			pthread_mutex_unlock(&pool->pool_mutex);
			break ;
		}
		else
		{
			pthread_cond_wait(&pool->pool_cond, &pool->pool_mutex);
			pthread_mutex_unlock(&pool->pool_mutex);
		}
	}
}


static unsigned int create_list_mtl_thread(mtl_pool *pool, unsigned int nb)
{
	unsigned int 	ret;
	unsigned int	i;
	unsigned int 	count;
	unsigned int 	start;

	pthread_mutex_lock(&pool->pool_mutex);
	pool->status = THREAD_EN_CREATION;//un thread ne quittera pas tant que cette valeur == à en creation
	start = pool->nb_thread;//forcement etat 0, un thread ne donne pas l'ordre de detruire.
	pthread_cond_broadcast(&pool->pool_cond);
	pthread_mutex_unlock(&pool->pool_mutex);
	i = 0;
	//on avance jusqu'au premier espace vide, le tab est forcément créé avant pour le nombre souhaité
	while (pool->tab_thread[i] != NULL)
			i++;
	count = 0;
	while (count < nb)
	{
		pool->tab_thread[i] = create_mtl_thread(i);
		if (pool->tab_thread[i] == NULL)
		{
			st_check_create_thread(count + start, pool);//start pour le nombre de thread initial
			return (count);
		}
		ret = pthread_create(&pool->tab_thread[i]->thread, NULL,
							mtl_action_thread, pool->tab_thread[i]);
		if (ret)
		{
			st_check_create_thread(count + start, pool);
			return (count);
		}
		i++;
		count++;
	}
	st_check_create_thread(count + start, pool);
	return (count);
}

static mtl_fct 			*create_mtl_fct(void)
{
	mtl_fct			*elem;

	elem = calloc(1, sizeof(mtl_fct));
	if (elem == NULL)
		return (NULL);

	elem->next = NULL;
	return (elem);
}
//a utilisé pour metttre les élément en debut de liste. de dest 


//retourne un pointeur sur la fin et pointe le debu a l'adresse indiquer
static mtl_fct 		*create_list_mtl_fct(unsigned int nb_fct, mtl_fct **dest, int *ret)
{
	mtl_fct 	*begin;
	mtl_fct 	*tmp;
	mtl_fct 	*last;
	unsigned int i;

	begin = create_mtl_fct();
	if (begin == NULL)
	{
		*ret = 0;
		return (begin);
	}
	i = nb_fct;
	last = begin;
	begin->next = *dest;
	nb_fct--;
	while (nb_fct > 0)
	{
		tmp = begin;
		begin = create_mtl_fct();
		if (begin == NULL)
		{
			begin = tmp;
			*dest = begin;
			*ret = i - nb_fct;
			return (last);
//			return (i - nb_fct);
		}
		begin->next = tmp;
		nb_fct--;
	}
	*dest = begin;
	*ret = i;
	return (last);//nombre d'élement effectivement créé.
}

static void			mtl_free_list_mtl_fct(mtl_fct **begin_list)
{
	mtl_fct 	*tmp;

	tmp = *begin_list;
	while (tmp)
	{
		tmp = tmp->next;
		free(*begin_list);
		*begin_list = tmp;
	}
	*begin_list = NULL;
}

/*
****************************Function ajustemlent des thread***********************
*/

static void 		st_add_new_list_fct(unsigned int nb, mtl_pool *pool)
{
	mtl_fct 		*list;
	mtl_fct 		*end;
	int 			ret;

	list = NULL;
	end = create_list_mtl_fct(NB_FCT_FOR_THREAD * nb, &list, &ret);
	if (ret != 0)
	{
		pthread_mutex_lock(&pool->pool_mutex);
		end->next = pool->list_free;
		pool->list_free = list;
		pthread_cond_broadcast(&pool->pool_cond);
		pthread_mutex_unlock(&pool->pool_mutex);
	}
}

static unsigned int st_add_thread(int nb_thread, mtl_glob *glob)
{
	mtl_thread 		**elem;
	mtl_thread 		**tmp;
	unsigned int 	nb_cur;
	unsigned int 	i;

	nb_cur = glob->pool->nb_thread;
	elem = calloc(nb_thread, sizeof(mtl_thread *));
	if (elem == NULL)
		return (nb_cur);
	i = 0;
	while (i < nb_cur)
	{
		elem[i] = glob->pool->tab_thread[i];
		i++;
	}
	pthread_mutex_lock(&glob->pool->pool_mutex);
	glob->pool->size_tab_thread = nb_thread;
	tmp = glob->pool->tab_thread;
	glob->pool->tab_thread = elem;
	pthread_mutex_unlock(&glob->pool->pool_mutex);
	st_memdel((void **)&tmp);

	st_add_new_list_fct(nb_thread - nb_cur, glob->pool);

	i = create_list_mtl_thread(glob->pool, nb_thread - nb_cur);
	return (i + nb_cur);
}

/*
**partie destruction de thread
*/

static void			st_check_exit(unsigned int i, unsigned int nb_cur, mtl_thread **tmp)
{
	while (i < nb_cur)
	{
		pthread_join(tmp[i]->thread, NULL);
		pthread_mutex_destroy(&tmp[i]->thread_mutex);
		st_memdel((void **)&tmp[i]);
		i++;
	}
	st_memdel((void **)&tmp);
}

static void			st_send_for_exit(mtl_fct *fct_exit, mtl_thread **tmp, unsigned int stop)
{
	unsigned int 	i;

	i = 0;
	while (i < stop)
	{
		pthread_mutex_lock(&tmp[i]->thread_mutex);
		mtl_push_back_mtl_fct(&tmp[i]->dedicated_list, 
								&tmp[i]->end_dedicated_list,
								fct_exit);
		pthread_mutex_unlock(&tmp[i]->thread_mutex);
		i++;
	}
}

static void 		st_del_mtl_fct(unsigned int nb, mtl_pool *pool, mtl_fct *begin)
{
	mtl_fct 		*tmp;

	tmp = begin;
	while (nb > 0)
	{
		tmp->next = mtl_search_block_free(pool);
		tmp = tmp->next;
		nb--;
	}
	mtl_free_list_mtl_fct(&begin);
}

static unsigned int st_del_thread(unsigned int nb_thread, mtl_glob *glob)
{
	mtl_thread 		**elem;
	mtl_thread 		**tmp;
	unsigned int 	nb_cur;
	unsigned int 	i;
	mtl_fct 		*fct_exit;

	nb_cur = glob->pool->nb_thread;
	elem = calloc(nb_thread, sizeof(mtl_thread *));
	if (elem == NULL)
		return (nb_cur);
	i = 0;
	while (i < nb_thread)
	{
		elem[i] = glob->pool->tab_thread[i];
		i++;
	}
	pthread_mutex_lock(&glob->pool->pool_mutex);
	tmp = glob->pool->tab_thread;//recupere le tableau courant avant nettoyage
	glob->pool->tab_thread = elem;//assigne le nouveau tableau
	glob->pool->nb_thread = nb_thread;
	glob->pool->size_tab_thread  = nb_thread;
	pthread_mutex_unlock(&glob->pool->pool_mutex);
	
	fct_exit = mtl_search_block_free(glob->pool);
	fct_exit->fct = (void *(*)())&pthread_exit;
	fct_exit->nb_arg = 1;
	fct_exit->argv[0] = 0;
	
	st_send_for_exit(fct_exit, &tmp[i], nb_cur - nb_thread);
	st_check_exit(i, nb_cur, tmp);
	st_del_mtl_fct(((nb_cur - nb_thread) * NB_FCT_FOR_THREAD) - 1, 
					glob->pool, fct_exit);
	return (nb_thread);
}

unsigned int	mtl_adjust_nb_thread(int nb_thread)
{
	mtl_glob *glob;

	glob = st_sig_mtl();
	if (nb_thread < 0)
		nb_thread = glob->nb_thread_hard - 1;

	if ((unsigned int)nb_thread < glob->pool->nb_thread)
		return (st_del_thread(nb_thread, glob));
	else if ((unsigned int)nb_thread > glob->pool->nb_thread)
		return (st_add_thread(nb_thread, glob));

	return (nb_thread);
}


/*
************************send function***************************
*/


static void	
st_check_param(va_list *ap, mtl_fct **tmp, int *status,
				int size_ret, int type_ret, void *dest, void *(*ft)(), 
				const unsigned int nb_param)
{
	unsigned int 		i;
	
	(*tmp)->status = status;
	(*tmp)->dest = dest;
	(*tmp)->fct = ft;
	(*tmp)->nb_arg = nb_param;
	(*tmp)->size_ret = size_ret;
	(*tmp)->type_ret = type_ret;
	i = 0;
	while (i < nb_param)
	{
		(*tmp)->argv[i] = va_arg(*ap, size_t);
		i++;
	}
}



static void 	mtl_push_front_mtl_fct(mtl_fct **dest, mtl_fct **end, mtl_fct *src)
{
	if (*dest)
		src->next = *dest;
	else
		*end = src;
	*dest = src;
}

static void			st_send(int thread, mtl_fct *tmp, mtl_pool *pool,
							void (*push)(mtl_fct **, mtl_fct **, mtl_fct *))
{
	if (thread < 0)
	{
		pthread_mutex_lock(&pool->pool_mutex);
		push(&pool->list_fct, &pool->end_list_fct, tmp);
		pthread_mutex_unlock(&pool->pool_mutex);
	}
	else
	{
		pthread_mutex_lock(&pool->tab_thread[thread]->thread_mutex);
		push(&pool->tab_thread[thread]->dedicated_list,
				&pool->tab_thread[thread]->end_dedicated_list,
		 		tmp);
		pthread_mutex_unlock(&pool->tab_thread[thread]->thread_mutex);
	}
}

void 			mtl_priority_send_function(int thread, int *status, int size_ret, int type_ret, void *dest, 
											void *(*ft)(), unsigned int nb_param, ...)
{
	mtl_glob 	*glob;
	va_list 			ap;
	mtl_fct 			*tmp;
	
	glob = st_sig_mtl();

	//gerer la fonction reçu en entré
	tmp = mtl_search_block_free(glob->pool);
	va_start(ap, nb_param);
	st_check_param(&ap, &tmp, status, size_ret, type_ret, dest, ft, nb_param);
	va_end(ap);
	st_send(thread, tmp, glob->pool, &mtl_push_front_mtl_fct);
}

void			mtl_send_function(int thread, int *status, int size_ret, int type_ret, void *dest, 
											void *(*ft)(), unsigned int nb_param, ...)
{
	mtl_glob 	*glob;
	va_list 			ap;
	mtl_fct 			*tmp;
	
	glob = st_sig_mtl();

	//gerer la fonction reçu en entré
	tmp = mtl_search_block_free(glob->pool);
	va_start(ap, nb_param);
	st_check_param(&ap, &tmp, status, size_ret, type_ret, dest, ft, nb_param);
	va_end(ap);
	st_send(thread, tmp, glob->pool, &mtl_push_back_mtl_fct);
}



int 			mtl_exec_one_share_function(void)
{
	mtl_glob 	*glob;

	glob = st_sig_mtl();
	return (mtl_check_share_list(glob->pool));
}

int 			mtl_exec_one_dedicated_function(unsigned int thread)
{
	mtl_glob 	*glob;

	glob = st_sig_mtl();
	return (mtl_thread_check_dedicated_fct(glob->pool, glob->pool->tab_thread[thread]));
}

int 			mtl_exec_one_function(unsigned int thread)
{
	mtl_glob 	*glob;

	glob = st_sig_mtl();
	if (mtl_thread_check_dedicated_fct(glob->pool, glob->pool->tab_thread[thread]) == 0)
		return (mtl_check_share_list(glob->pool));
	return (1);
}


/*
*********function synchro*********************************
*/


static mtl_join 	*create_mtl_join(void)
{
	mtl_join		*elem;

	elem = calloc(1, sizeof(mtl_join));
	if (elem == NULL)
		return (NULL);

	return (elem);
}

mtl_join 		*mtl_init_join(mtl_join *list, int *status)
{
	mtl_join 	*elem;
	mtl_glob 	*glob;

	glob = st_sig_mtl();
	pthread_mutex_lock(&glob->glob_mutex);
	if (glob->list_join)
	{
		elem = glob->list_join;
		glob->list_join = glob->list_join->next;
		pthread_mutex_unlock(&glob->glob_mutex);
	}
	else
	{
		pthread_mutex_unlock(&glob->glob_mutex);
		elem = create_mtl_join();
		if (elem == NULL)
			return (NULL);
	}
	elem->next = list;
	elem->status = status;
	return (elem);
}

static mtl_join 	*st_dedicated_join(mtl_join *tmp, mtl_pool *pool, 
										mtl_thread *thread, unsigned int *i)
{
	mtl_join 		*last;

	last = tmp;
	while (tmp)
	{
		if (*(tmp->status) == 0)
		{
			if (mtl_thread_check_dedicated_fct(pool, thread) == 0)
				mtl_check_share_list(pool);
		}
		else
		{
			(*i)++;
			last = tmp;
			tmp = tmp->next;
		}
	}
	return (last);
}

static mtl_join 	*st_share_join(mtl_join *tmp, mtl_pool *pool, unsigned int *i)
{
	mtl_join 		*last;

	last = tmp;
	while (tmp)
	{
		if (*(tmp->status) == 0)
			mtl_check_share_list(pool);
		else
		{
			(*i)++;
			last = tmp;
			tmp = tmp->next;
		}
	}
	return (last);
}	


static void 	st_clean_too_much_join(mtl_join *list, unsigned int nb, 
										mtl_glob *glob)
{
	mtl_join 	*tmp;
	mtl_join 	*start;

	start = list;
	while (nb > 0)
	{
		tmp = list;
		list = list->next;
		nb--;
	}
	glob->nb_join = NB_JOIN_MAX;
	glob->list_join = list;
	pthread_mutex_unlock(&glob->glob_mutex);
	tmp->next = NULL;
	tmp = start;
	list = start;
	while (tmp)
	{
		tmp = tmp->next;
		free(list);
		list = tmp;
	}
}

void 			mtl_join_status(mtl_join *list, int num_thread)
{
	mtl_glob			*glob;
	mtl_join 			*tmp;
	unsigned int 		i;

	glob = st_sig_mtl();
	i = 0;
	if (num_thread < 0)//check only file partager
		tmp = st_share_join(list, glob->pool, &i);
	else
		tmp = st_dedicated_join(list, glob->pool, glob->pool->tab_thread[num_thread], &i);
	pthread_mutex_lock(&glob->glob_mutex);
	tmp->next = glob->list_join;
	glob->list_join = list;
	glob->nb_join += i;
	if (glob->nb_join > NB_JOIN_MAX	)
		st_clean_too_much_join(glob->list_join, glob->nb_join - NB_JOIN_MAX, glob);
	else
		pthread_mutex_unlock(&glob->glob_mutex);
}

/*
*****function init
*/

static mtl_pool	*create_mtl_pool(const unsigned int nb_thread)
{
	mtl_pool 	*pool;
	int 		ret;

	pool = malloc(sizeof(mtl_pool));
	if (pool == NULL)
		return (NULL);

	pool->status = 0;
	pool->nb_thread_to_destroy = 0;
	pool->size_tab_thread = nb_thread;
	pool->nb_thread = 0;
	pool->list_fct = NULL;
	pool->end_list_fct = NULL;
	pool->list_free = NULL;
	/*pool->size_list_total = */
	create_list_mtl_fct((NB_FCT_FOR_THREAD * nb_thread) + NB_FCT_FOR_THREAD,
						&pool->list_free, &ret);	
	if (ret == 0)
	{
		st_memdel((void **)&pool);
		return (NULL);
	}

	if (pthread_mutex_init(&pool->pool_mutex, NULL))
	{
		st_memdel((void **)&pool->list_free);
		st_memdel((void **)&pool);
		return (NULL);
	}

	if (pthread_cond_init(&pool->pool_cond, NULL))
	{
		pthread_mutex_destroy(&pool->pool_mutex);
		st_memdel((void **)&pool->list_free);
		st_memdel((void **)&pool);
		return (NULL);
	}

	if (nb_thread > 0)
	{
		pool->tab_thread = calloc(nb_thread, sizeof(mtl_thread *));
		if (pool->tab_thread == NULL)
		{
			pthread_cond_destroy(&pool->pool_cond);
			pthread_mutex_destroy(&pool->pool_mutex);
			st_memdel((void **)&pool->list_free);
			st_memdel((void **)&pool);
			return (NULL);
		}
	}
	else
	{
		pool->tab_thread = NULL;
	}

	return (pool);
}

static unsigned int 	st_check_nb_thread(const int code,
											const unsigned int nb_hard_thread)
{
	if (code < 0)
		return (nb_hard_thread - 1);
	else
		return (code);
}

static mtl_glob *st_init_data_glob(const int nb_thread)
{
	mtl_glob 	*glob;
	unsigned int nb_to_create;

	glob = st_sig_mtl();
	glob->nb_thread_hard = sysconf(_SC_NPROCESSORS_CONF);
	if (glob->nb_thread_hard == -1)
		return (NULL);
	glob->list_join = NULL;
	glob->nb_join = 0;
	if (pthread_mutex_init(&glob->glob_mutex, NULL))
		return (NULL);
	nb_to_create = st_check_nb_thread(nb_thread, glob->nb_thread_hard);
	glob->pool = create_mtl_pool(nb_to_create);
	if (glob->pool == NULL)
	{
		pthread_mutex_destroy(&glob->glob_mutex);
		return (NULL);
	}
	return (glob);
}

int 			mtl_init(const int nb_thread)
{
	mtl_glob 	*glob;

	glob = st_init_data_glob(nb_thread);
	if (glob == NULL)
		return (-1);
	//j'ai créé la structure, maintenant il faut la remplir de thread si il y a des thread a créer
	if (glob->pool->size_tab_thread > 0)
	{
		glob->pool->nb_thread = create_list_mtl_thread(glob->pool,
													 glob->pool->size_tab_thread);
		if (glob->pool->nb_thread == 0)
		{
			st_memdel((void **)&glob->pool->tab_thread);
			pthread_cond_destroy(&glob->pool->pool_cond);
			pthread_mutex_destroy(&glob->pool->pool_mutex);
			st_memdel((void **)&glob->pool->list_free);
			st_memdel((void **)&glob->pool);
			return (-1);
		}
	}
	return (glob->pool->nb_thread);
}

/*
****function end**********************
*/

static void 		free_full_mtl_thread(mtl_thread ***thread, const unsigned int len)
{
	unsigned int i;

	i = 0;
	while (i < len)
	{
		st_memdel((void **)&((*thread)[i]));
		i++;
	}
	st_memdel((void **)thread);
}

static void		st_clean_list_join(mtl_join **begin_list)
{
	mtl_join 	*tmp;

	tmp = *begin_list;
	while (tmp)
	{
		tmp = tmp->next;
		free(*begin_list);
		*begin_list = tmp;
	}
}

static void		st_clean_mtl(mtl_glob *glob)
{
	mtl_free_list_mtl_fct(&glob->pool->list_free);
	if (glob->pool->size_tab_thread > 0)
		free_full_mtl_thread(&glob->pool->tab_thread, glob->pool->size_tab_thread);
	pthread_cond_destroy(&glob->pool->pool_cond);
	pthread_mutex_destroy(&glob->pool->pool_mutex);
	st_memdel((void **)&glob->pool->list_free);
	st_memdel((void **)&glob->pool);
	if (glob->list_join)
		st_clean_list_join(&glob->list_join);
	pthread_mutex_destroy(&glob->glob_mutex);
}

void 			mtl_end(void)
{
	mtl_glob 	*glob;
	unsigned int i;

	/*checker les fonctions existantes en parrallele
	
	*/
	glob = st_sig_mtl();
	pthread_mutex_lock(&glob->pool->pool_mutex);
	glob->pool->status = THREAD_TO_DESTROY;//etant utilisé par le main, il ne peut etre qu'a zero ici.
	pthread_cond_broadcast(&glob->pool->pool_cond);
	pthread_mutex_unlock(&glob->pool->pool_mutex);
	while (mtl_check_share_list(glob->pool))
		;
	i = 0;
	while (i < glob->pool->size_tab_thread)
	{
		pthread_join(glob->pool->tab_thread[i]->thread, NULL);
		i++;
	}
	st_clean_mtl(glob);
}