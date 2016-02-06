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

#ifndef MTLIB_H
# define MTLIB_H

#	if defined (WIN32)
# 		include <windows.h>
#	else
#		include <unistd.h>
#	endif

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

# define 		NB_THREAD_STD 			-1

# define 		STD						-1

# define 		NO_STATUS				&mtl_no_status
# define 		NO_RET 					mtl_no_ret
# define 		PTR 					1
# define 		VALUE					0


typedef struct 			s_mtl_join
{
	int 				*status;
	struct s_mtl_join 	*next;
}						mtl_join;


unsigned int	mtl_adjust_nb_thread(int nb_thread);
int 			mtl_init(const int nb_thread);
mtl_join 		*mtl_init_join(mtl_join *list, int *status);
void 			mtl_join_status(mtl_join *list, int num_thread);
int 			mtl_exec_one_share_function(void);//1 == une fonction executé, 0 == aucune
int 			mtl_exec_one_dedicated_function(unsigned int thread);
int 			mtl_exec_one_function(unsigned int thread);

void 			mtl_end(void);
void			mtl_send_function(int thread, int *status, int size_ret, int type_ret, void *dest,
											void *(*ft)(), unsigned int nb_param, ...);
void 			mtl_priority_send_function(int thread, int *status, int size_ret, int type_ret, void *dest,
											void *(*ft)(), unsigned int nb_param, ...);


# define 		MTL_SEND(thread, status, type_ret, dest, fct, ...)							mtl_send_function(thread, status, sizeof(*dest), type_ret, (void *)dest, (void *(*)())fct, __VA_ARGS__)
# define 		MTL_SEND_NO_RET(thread, status, fct, ...)									mtl_send_function(thread, status, sizeof(*(NO_RET)), VALUE, NO_RET, (void *(*)())fct, __VA_ARGS__)
# define 		MTL_PRIORITY_SEND(thread, status, type_ret, dest, fct, ...)					mtl_priority_send_function(thread, status, sizeof(*dest), type_ret, (void *)dest, (void *(*)())fct, __VA_ARGS__)
# define 		MTL_PRIORITY_SEND_NO_RET(thread, status, fct, ...)							mtl_priority_send_function(thread, status, sizeof(*(NO_RET)), VALUE, NO_RET, (void *(*)())fct, __VA_ARGS__)

#endif /* !MTLIB_H */
