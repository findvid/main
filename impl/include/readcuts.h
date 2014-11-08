#include <stdio.h>
#include <stdlib.h>

typedef struct {
	char type;
	int start;
	int end;
} CutInfo;

/*
Format of the file:
AMOUNT_OF_CUTS
t1 s1 e1
t2 s2 e2
...
tn sn en

where t is a char giving the type of cut
and s and e the start of the end frame of the cut

t can be:
C:	CUT
D:	DIS
F:	FOI
O:	OTH
*/


CutInfo* readCutInfo(char *filename, int *amountread) {
	char line[100];
	FILE *f = fopen(filename, "rt");
	CutInfo *retval;
	int i = 0;

	if (fgets(line, 100, f)) {
		sscanf(line, "%d", amountread);
	} else {
		printf("readCutInfo(): File empty?\n");
		*amountread = 0;
		return NULL;
	}
	retval = (CutInfo *)calloc(sizeof(CutInfo), *amountread);
	if (!retval) {
		printf("malloc failed in readCutInfo()\n");
		exit(1);
	}
	while (fgets(line, 100, f)) {
		if (i >= *amountread) {
			printf("readCutInfo(): To many lines in File\n");
			*amountread = 0;
			return NULL;
		}
		sscanf(line, "%c %d %d", &(retval[i].type), &(retval[i].start), &(retval[i].end));
		switch (retval[i].type) {
			case 'C':
			case 'D':
			case 'F':
			case 'O':
				break;
			default:
				printf("readCutInfo(): Unknown cut type '%c'\n", retval[i].type);
				*amountread = 0;
				return NULL;
		}
		i++;
	}
	if (i != *amountread) {
		printf("readCutInfo(): To few lines in File\n");
		*amountread = 0;
		return NULL;
	}
	fclose(f);
	return retval;
}

/*int main(int argc, char *argv[]) {
	int cut_count = 0;
	CutInfo* cuts = readCutInfo(argv[1], &cut_count);
	int i;
	printf("%d Cuts have been read:\n", cut_count);
	for (i = 0; i < cut_count; i++) {
		printf("Type: %c, Cut %d starts at %d and ends at %d\n", cuts[i].type, i, cuts[i].start, cuts[i].end);
	}
	return 0;
}*/
