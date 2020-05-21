#define BUFFER_SIZE 200
#define WORD 50

typedef struct makefile {
	char target[WORD];
	char dependency[WORD][WORD];
	char command[WORD][WORD];
} makefile;

typedef struct macro_value {
	char macro[WORD];
	char value[WORD];
} macro_value;

typedef struct manage {
	int info_num;
	int dep_num[WORD];
	int macro_num;
	int commnd_num[WORD];
} manage;
