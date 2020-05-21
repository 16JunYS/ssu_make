#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "ssu_make_1.h"

//#define DEBUG_info
//#define DEBUG_macro
//#define DEBUG_COMMAND

int fd; //file descriptor of Makefile
char* filename = "Makefile";
int command = 0; //1 : command 실행 안됨
int s_opt = 0; //option -s
int printM = 0; //option -m
int printG = 0; //option -t
int level=0;
int circ[WORD] = {0}; //의존성 그래프 순환 확인

manage ssu; //Makefile의 target 수와 macro 관리

void openFile(); //open Makefile
int checkMkfile(); // check Makefile error
void categorize (makefile *info, macro_value *macroInfo);
int SSUmake(makefile *Info, macro_value *macroInfo, char *usr_tar, int prev); //execute Makefile
void getTARGET(int, char *argv[], char (*usr_tar)[], macro_value *usr_mac); //read target from user
void OPTION(int, char* argv[]);//process option
void PrintUsage();//option h
void printMacro(macro_value *macroInfo);//option m
void printGraph(makefile *Info);//optino t


int main(int argc, char* argv[]) 
{
	int error;
	int i;
	makefile Info[100] ={'\0'}; //파일 각 target 별로 구조체에 의존성, command 저장
	macro_value Macro[100] = {'\0'}; //매크로와 그 값 저장

	char usr_tar[5][WORD] = {'\0'}; //사용자가 입력한 target 저장
	char circular[BUFFER_SIZE] = {'\0'}; //의존성 그래프의 순환 확인
	OPTION(argc, argv);	
	openFile();
	error = checkMkfile();
	if(error) exit(1); //end process

	//classify target, dependency, command, macro
	categorize(Info, Macro);
	//find target
	getTARGET(argc, argv, usr_tar, Macro);

	if(!command) {	

		for(i=0; i<WORD; i++) {
			if(usr_tar[i][0] != '\0' && usr_tar[i][0] != '.') {
				SSUmake(Info, Macro, usr_tar[i], 0);
			}
			else if(i==0 && ( usr_tar[i][0] == '.' || usr_tar[i][0] == '\0')) {
				SSUmake(Info, Macro, Info[0].target, 0); //target 없는 경우 기본 값으로 실행
			}
			else break;
		}

	}
	else {
		if(printM) printMacro(Macro);
		if(printG) printGraph(Info);
	}


	exit(0);
}

void openFile() 
{
	//int error;
	if((fd = open(filename, O_RDONLY))<0 && !s_opt) { //Makefile이 없는 경우 에러 출력
		if(!s_opt)fprintf(stderr, "make: %s: No such file or directory\n", filename);
		exit(1);
	}
} 

int checkMkfile()
{
	off_t filesize = lseek(fd, (off_t)0, SEEK_END); //get size of fd

	char character; //파일로부터 1byte씩 character에 저장
	char line[BUFFER_SIZE] = {'\0'}; // 파일 한 문장 line에 저장
	int lineNum = 0; //줄 번호
	int tar_com = 0;
	int error = 0; //error가 있는 경우 1

	int i = 0;
	int a = 0;

	lseek(fd, 0, SEEK_SET); //파일의 offset 시작점으로 변경
	while(read(fd, &character, 1) > 0) {
		line[i] = character;

		//공백으로 시작하는 경우
		if(line[0] == ' ') {
			error = 1;
			if(!s_opt)fprintf(stderr, " %s:%d: *** missing separator. Stop.\n", filename, lineNum);
			break;
		}

		if(line[i] == ':') tar_com = 1;
		if(line[i] == '=') tar_com = 0;

		//긴 문장 처리 '\'
		if(line[i] == '\\') {
			if((read(fd, &character, 1)) < 0) {
				error = 1;
				if(!s_opt)fprintf(stderr, "%s:%d: *** missing separator. Stop.\n", filename, lineNum);
				break;
			}
			if (character == '\n');  //다음 문장 받음
			else { // '\' 문자 뒤에 다른 문자가 오는 경우 에러
				error = 1;
				if(!s_opt)fprintf(stderr, "%s:%d: *** missing separator. Stop.\n", filename, lineNum);
				break;
			}
		}

		//한 문장이 끝난 경우
		else if(line[i] == '\n') {
			lineNum++; 

			if(line[0] == '	') { //'\t'으로 시작한 경우
				if(!tar_com) { //command가 아닌 경우 error
					error = 1;
					printf("%s:%d: *** missing separator. Stop.\n", filename, lineNum);

				}
			}

			for(a = 0; a <= i; a++) line[a]='\0'; //다음 문장 받기 위해 line 문자열 초기화
			i = 0;
		}
			else i++;
		}	
		return error;
	}

	void categorize(makefile *info, macro_value *macroInfo) 
	{
		char buf[BUFFER_SIZE]; //파일 한 문장씩 line에 저장
		char token[WORD][WORD] = {'\0'}; //공백 기준으로나눠진 단어 저장
		char *TOK[WORD] = {'\0'}; //공백 기준으로 나눈 단어의 주소값 저장
		char character;
		char change_buf[WORD] = {'\0'}; //매크로의 value값으로 바꾼 후 해당 문장
		char change_mac[WORD] = {'\0'}; //바꿔야 할 매크로


		ssu.info_num = 0; //initialize number of targets
		ssu.macro_num = 0; //initialize number of macros

		int tok_num; // 한 문장에서 공백 기준으로 나누어진 단어 수
		int i = 0; //iteration
		int a = 0; 
		int letter; //한 글자씩 읽을 때 순서를 나타냄
		int info_i = 0; //target 수
		int tok = 0;
		int dep_num = 0; //dependency number
		int command = 0;
		int com_num = 0; //한 target에서 command 순서
		int com_continue = 0; //같은 target에 해당하는 command인지 아닌지 확인용
		int continue_sent = 0; //긴 문장 처리 된 경우
		int t=0;
		int mac_start, mac_end; //매크로 시작부분과 끝 부분 배열 번호 저장
		int no_macro = 0;
		lseek(fd, 0, SEEK_SET); //파일의 offset 시작점으로 설정

		while(read(fd, &character, 1) > 0) {
			buf[i] = character;	

			if(buf[i] == '\\') {
				read(fd, &character, 1); //skip '\n'
			}

			else if(buf[i] == '\n') {
			
				///Makefile 주석처리
				if(buf[0] == '#') {
					for(a=0; a <= i; a++) buf[a] = '\0';
					continue;
				}

				else if(buf[0] == '	' || buf[0] == '\t') { //'\t'로 구분 된 command인 경우
					if(com_continue == 0) { //한 target에 대해서 첫번째 command인 경우
						com_num = 0;
					}
					
					for(a=0, i=1; i<strlen(buf); i++) {
						if(buf[i] == '$') { //매크로가 있는 경우
							no_macro = 0;
							mac_start = i++; //매크로 시작 부분 index 값 저장
							if(buf[i] == '(') { //Makefile에 정의되어 있는 매크로인 경우
								i++;
								while(buf[i] != ')') {
									change_mac[a++] = buf[i++];
								}
								mac_end = i;
								for(a = 0; a < ssu.macro_num; a++) {
									if(strcmp(change_mac, macroInfo[a].macro) == 0) { //해당 매크로가 정의되어 있는 지 확인
									//change_buf에 매크로 값으로 바꿔서 저장
										strncpy(change_buf, buf+1, mac_start-1);
										strcat(change_buf, macroInfo[a].value);
										strcat(change_buf, buf+(mac_end +1));
									}
								}
								//해당 macro->value 값으로 바꿔서 info구조체에 저장
								strcpy(info[ssu.info_num-1].command[com_num++], change_buf);

								for(a = 0; a < WORD; a++) { //intialize chagne_buf
									change_buf[a] = '\0';
								}
							}
							else if(buf[i] == '*') { //내부매크로($*) : 확장자가 없는 현재의 목표 파일	
								mac_end = i;
								i = 0;
								while(info[ssu.info_num-1].target[i] != '.') {
									change_mac[a++] = info[ssu.info_num-1].target[i++];
								}
								
								strncpy(change_buf, buf+1, mac_start-1);
								strcat(change_buf, change_mac);
								strcat(change_buf, buf+(mac_end+1));

								//$* 내부 매크로->확장자 없는 현재 target파일로 바꿔서 info구조체에 저장
								strcpy(info[ssu.info_num-1].command[com_num++], change_buf);

								for(a = 0; a < WORD; a++) { //initailize change_buf
									change_buf[a] = '\0';
								}
							}
							else if(buf[i] == '@') {;//내부 매크로($@) : 현재 목표 target
								mac_end = i;
								strcpy(change_mac, info[ssu.info_num-1].target);

								strncpy(change_buf, buf+1, mac_start-1);
								strcat(change_buf, change_mac);
								strcat(change_buf, buf+(mac_end+1));

								//$@ 내부 매크로->현재 target 파일로 바꿔서 info구조체에 저장
								strcpy(info[ssu.info_num-1].command[com_num++], change_buf);

								for(a = 0; a < WORD; a++) { //initailize change_buf
									change_buf[a] = '\0';
								}
							}
							else no_macro = 1;	
							break;
						}
						else no_macro = 1;
					}
				
					if(no_macro) { //매크로 없는 경우
						strcpy(info[ssu.info_num-1].command[com_num++], buf+1);
					}
					com_continue = 1;
					command = 1;
					ssu.commnd_num[ssu.info_num-1] = com_num;
				}
				else {
					//	command = 0;
					buf[i] = '\0';//개행문자 대신 NULL 추가
					a = 0;
					TOK[a] = strtok(buf, " ");
					while(TOK[a] != NULL) { //" "기준으로 buf 잘라서 주소값 TOK[a]에 저장
						strcpy(token[a], TOK[a]);
						a++;
						TOK[a] = strtok(NULL, " ");
					}
					tok_num = a;			

					for(tok = 0; tok < tok_num; tok++) {
						////target과 dependency 정의
						if(token[tok][0] == ':') { 
							com_continue = 0;
							t = 1; //':'앞 뒤 공백이 있는 경우
							dep_num = 0;
							strcpy(info[ssu.info_num].target, token[tok-1]);

							strcpy(info[ssu.info_num].dependency[dep_num++], token[++tok]);

							while(++tok != tok_num) {
								strcpy(info[ssu.info_num].dependency[dep_num++], token[tok]);
							}
							ssu.dep_num[ssu.info_num] = dep_num; //target에 해당하는 dependecy 개수 저
							ssu.info_num++; //Makefile 내 target 수++
							break;
						}

						//macro 정의장
						else if(token[tok][0] == '=' || token[tok][0] == '?') {
							t = 1; //'=''?='앞 뒤 공백이 있는 경우
							strcpy(macroInfo[ssu.macro_num].macro, token[tok-1]);
							strcpy(macroInfo[ssu.macro_num].value, token[++tok]);
							ssu.macro_num++;
							break;
						}
						else t = 0; //token에 '='':'이 없는 경우
					}	
					//token에 target,':',dependency모두 포함되어 있는 경우
					if (!t) {	
						for(tok = 0; tok < tok_num; tok++) {
							for(letter = 0; letter < sizeof(token[tok]); letter++) {
								//macro 정의
								if(token[tok][letter] == '=' || token[tok][letter] == '?') {
									for(a = 0; a < letter; a++){ //macro 1byte씩 저장
										macroInfo[ssu.macro_num].macro[a] = token[tok][a];
									}
									for(a = letter+1; a < sizeof(token[tok]); a++) { //value 값 1byte씩 저장
										if(token[tok][letter] == '?')
											macroInfo[ssu.macro_num].value[a-(letter+1)] = token[tok][a+1];
										else
											macroInfo[ssu.macro_num].value[a-(letter+1)] = token[tok][a];
									}
									ssu.macro_num++;
									break;
								}

								//target과 dependency 정의
								else if(token[tok][letter] == ':') {	
									com_continue = 0;
									for(a = 0; a < letter; a++){ //macro 1byte씩 저장
										info[ssu.info_num].target[a] = token[tok][a];
									}
									for (a = letter+1; a < sizeof(token[tok]); a++) { //dependency 1byte씩 저장
										dep_num = 0;
										info[ssu.info_num].dependency[dep_num++][a-(letter+1)] = token[tok][a];
									}
									while(++tok != tok_num) {
										strcpy(info[ssu.info_num].dependency[dep_num++], token[tok]);
									}
									ssu.dep_num[ssu.info_num] = dep_num;
									ssu.info_num++;
									break;
								}
								else; //아직 ':''='등 기호가 안나온 경우
							}
						}
					}
				}

				for(i=0; i<WORD; i++) {
					buf[i] = '\0';
				}
				i = 0;
			}
			else i++;
		}

		if(ssu.info_num == 0) {
			if(!s_opt)printf("make: *** No targets. Stop.\n");
			exit(0);
		}

#ifdef DEBUG_info
		printf("\n");
		for(i=0; i < ssu.info_num; i++) {
			printf("#%d TARGET: %s, DEPENDENCY :",i, info[i].target);
			for(a=0; a < WORD; a++) {
				printf("%s	", info[i].dependency[a]);
			}
			printf("\n");
		}
#endif

#ifdef DEBUG_macro
		for(i=0; i < ssu.macro_num; i++) {
			printf("#%d MACRO: %s = %s\n",i, macroInfo[i].macro, macroInfo[i].value);
		}
#endif

#ifdef DEBUG_COMMAND
		for(i=0;i < ssu.info_num; i++) {
			for(a=0; a < ssu.commnd_num[i]; a++ ) {
				printf("#%d COMMAND : %s", i, info[i].command[a]);
			}
		}
#endif
	}

	void getTARGET(int argc, char *argv[], char (*target)[WORD], macro_value *Macro)
	{
		int i;
		int tar_num = 0;
		int mac_num = 0;
		int a;
		//int mac_i = 0;
		int j = 0;
		char buf[WORD] = {'\0'};
		char findmac[WORD] = {'\0'};
		char usr_value[WORD] = {'\0'};

		for(i = 1; i < argc; i++) {
			j = 0;
			if(argv[i][0] == '-'); //옵션이므로 다음 단어로 찾음
			else {
				if (argv[i-1][0] != '-' && (argv[i-1][1] != 'f' || argv[i-1][1] != 'c')) {

					while(argv[i][j] != '\0') {
					////macro 사용자 정의////
						if(argv[i][j] == '=') {
							strncpy(findmac, buf, j);

							for(a = 0; a < ssu.macro_num; a++) {
								if(strcmp(findmac, Macro[a].macro) == 0) //Makefile에 macror가 이미 있는 경우
									break;
							}
							mac_num = a;
							if(a == ssu.macro_num) { //새로운 매크로 저장
								ssu.macro_num++;
								strcpy(Macro[mac_num].macro, findmac);
							}
							else { //기존의 macro value 값 초기화
								for(a = 0; a <= WORD; a++) {
									Macro[mac_num].value[a] = '\0';
								}
							}
							for(a = j+1; a < strlen(argv[i]); a++) { //새로운 value 저장
								Macro[mac_num].value[a-(j+1)] = argv[i][a];
							}
							break;
						}
					////argv[i][j]  저장////
						else if(j+1 != strlen(argv[i])){
							buf[j] = argv[i][j];
							j++;
						}
											
					////사용자가 입력한 target 저장
						else {
							buf[j] = argv[i][j];
							strcpy(target[tar_num], buf);
							tar_num++;
							break;
						}
					}
				}
			}	
		}
	}
	int SSUmake(makefile *Info, macro_value *macroInfo, char *usr_tar, int prev)
	{
		int tar = 0; //target 배열 번호
		int i, j, a;//iteration
		int new_tnum;
		//전역변수 circ[WORD] 사용

		////Makefile에서 해당되는 target 찾기////
		//target에 매크로 포함된 경우
		while(strcmp(usr_tar, Info[tar].target) != 0) {
			tar++;
			//해당 target을 찾지 못한경우
			// 현재 디렉토리에 있는지 확인. 없는 경우 error

			if(tar == ssu.info_num) { 
				
				if(level == 0 && !s_opt) printf("make: *** No rule to make target '%s'. Stop.\n", usr_tar);
				return -1; 
			}
		}

		if(circ[tar] == 1) {
			if(!s_opt) printf("make : Circular %s <- %s dropped.\n", Info[prev].target, Info[tar].target);
			return -1;
		}
		circ[tar] = 1; //1 : 해당 target 방문
		
		for(i = 0; i < ssu.dep_num[tar]; i++) {
			//dependency의 이름을 가진 target으로 SSUmake() 호출
			level++;
			new_tnum = SSUmake(Info, macroInfo, Info[tar].dependency[i],tar);
			if(new_tnum != -1) circ[new_tnum] = 0; //방문한 dependency 반환 (0)
		}	
		for(j = 0; j < ssu.commnd_num[tar]; j++) {
			if(!s_opt) printf("%s", Info[tar].command[j]);
			system(Info[tar].command[j]);
		}

		return tar;
	}
	void OPTION(int OptNum, char* option[]) 
	{
		int c;

		while((c = getopt(OptNum, option, "f:c:shmt")) != -1) {
			switch(c) {
				case 'f': //option f
					filename  = optarg;
					break;

				case 'c': //option c
					chdir(optarg);
					//확인
					char buf[30];
					getcwd(buf, 30);
					break;

				case 's'://option s
					s_opt = 1;
					break;

				case 'h': //option h
					PrintUsage();
					exit(0);
					break;
				case 'm'://option m
					printM = 1;
					command = 1;
					break;
				case 't'://option t
					printG = 1;
					command = 1;
					break;
			}
		}
	}

	void PrintUsage()
	{
		printf("Usage : ssu_make [Target] [Option] [Macro]\n");
		printf("Option :\n");
		printf(" -f <file>		Use <file> as a makefile.\n");
		printf(" -c <directory>	Change to directory <directory> before reading the makefiles.\n");
		printf(" -s				Do not print the commands as the are executed.\n");
		printf(" -h				print usage\n");
		printf(" -m				print macro list\n");
		printf(" -t				print tree\n");
	}

	void printMacro(macro_value *macroInfo)
	{
		int i;
		printf("--------------------------macro list-----------------------------\n");
		for(i = 0; i < ssu.macro_num; i++) {
			printf("%s -> %s\n", macroInfo[i].macro, macroInfo[i].value);
		}
	}

	void printGraph(makefile *Info)
	{
		/*
		   print dependency graph
		 */
	}
