#include "revert_string.h"

#include <string.h>

void RevertString(char *str)
{
	size_t len = strlen(str);
	if (len < 2)
		return;

	size_t i = 0;
	size_t j = len - 1;
	while (i < j)
	{
		char tmp = str[i];
		str[i] = str[j];
		str[j] = tmp;
		i++;
		j--;
	}
}
