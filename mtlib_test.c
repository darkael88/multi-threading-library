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

#include <mtlib.h>
#include <string.h>
#include <stdio.h>

# define NB 		100
//#define TEST

int 			ft_test3(int i)
{
	double 		j = 0;

	while (j < 500000)
	{
		j += 0.01;
	}
	return (i);
}


char 		*ft_test4(char *str)
{
	char 	*tab;

	tab = strdup(str);
	return (tab);
}

# ifndef TEST

int 			main()
{
	unsigned int 	i;
	int  			for_thread[NB];
	int 			status[NB] = {0};
	mtl_join 		*join = NULL;

	printf("initial : %d\n", mtl_init(STD));

	i = 0;

	while (i < NB)
	{
		MTL_SEND(STD, &status[i], VALUE, &for_thread[i], ft_test3, 1, i);
		join = mtl_init_join(join, &status[i]);
		i++;
	}
//	printf("now : %d\n", mtl_adjust_nb_thread(STD));
	mtl_join_status(join, STD);
	mtl_end();
	i = 0;
	while (i < NB)
	{
		printf("%d\n", for_thread[i]);//truc[i], for_thread[i]);
		i++;
	}
	return (0);
}

#else


int 			main()
{
	unsigned int 	i;
	int 			value[NB] = {0};


	i = 0;
	while (i < NB)
	{
		value[i] = ft_test3(i);
		i++;
	}
	i = 0;
	while (i < NB)
	{
		printf("%d\n", value[i]);
		i++;
	}
	return (0);
}

#endif
