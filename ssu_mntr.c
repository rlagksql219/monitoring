#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <syslog.h>

#define MAX 256
#define BUF_SIZE 1024
#define SECOND_TO_MICRO 1000000

void cmd_delete(char *filename, char endtime[], char buf[]); /* delete 명령어 수행 */
void cmd_recover(char *filename, char buf[]); /* recover 명령어 수행 */
void cmd_tree(); /* tree 명령어 수행 */
void cmd_help(); /* help 명령어 수행 */

void delete_action(char *filename, char oldpath[], char newpath[], char buf[]); /* 파일을 trash 디렉터리로 옮기고, info 파일 생성 */

int my_scandir(char *wd, int depth, char *filename); /* 인자로 받은 파일에 대한 하위 파일 모두 검색 */
void make_tree(char *file); /* 재귀적으로 호출되는 file을 인자로 받아 tree 구조 생성 */
void extra_function_scandir(char *wd, void (*func)(char *), int depth); /* 인자로 받은 파일에 대한 하위 파일을 모두 검색하고, 각각의 파일에 대한 함수 실행 */
int calculate_infofile_size(); /* info 파일 크기 계산 */
void delete_oldfile(off_t infofile_size); /* info 파일에서 젤 삭제한지 오래된 파일을 삭제 */
void make_original_list(char *file); /* 인자로 받은 file로 디렉토리 목록 original list 생성 */
void make_new_list(char *file); /* 인자로 받은 file로 디렉토리 목록 new list 생성 */

char* remove_space(char *str); /* 문자열에 대한 이스케이프 시퀀즈 제거 */
char* print_time(struct tm* t); /* 시간에 대한 구조체를 인자로 받아 년월일 시분초 형태로 출력 */
void eliminate_certain_character(char *str, char ch); /* 문자열에서 인자로 받은 특정 문자를 제거 */

int ssu_daemon(void); /* 데몬 프로세스를 생성하여 파일의 삭제, 생성, 수정을 모니터링하며 log 남기기 */
void ssu_runtime(struct timeval *begin_t, struct timeval *end_t); /* ssu_mntr의 수행시간 측정 */


static int indent = 0; //깊이 체크

char original_list[MAX][MAX] = {0,};
int original_list_cnt = 0;

char new_list[MAX][MAX] = {0,};
int new_list_cnt = 0;

char original_path[MAX];
char new_path[MAX];

char original_time[MAX][MAX];
char new_time[MAX][MAX];

int main()
{
    char buf[MAX]={0,};
    char *cpy = malloc(strlen(buf)+1);
    char *cmd;
    char *filename;
    char *endtime_day;
    char *endtime_time;
    char endtime[MAX]={0,};
    pid_t pid;

    struct timeval begin_t, end_t;

    gettimeofday(&begin_t, NULL);

    if ((pid = fork()) < 0) { //데몬 프로세스 생성
        fprintf(stderr, "fork error\n");
        exit(1);
    }

    else if (pid == 0) { //자식
        if (ssu_daemon() < 0) { //데몬 코딩 규칙
            fprintf(stderr, "ssu_daemon failed\n");
            exit(1);
        }
    }


    else //부모
        ;


    while (1) { //무한 반복하며 명령어 입력받음

        printf("20180737>"); //프롬프트 출력
        fgets(buf, MAX, stdin);

        strcpy(cpy, buf);
        cmd = strtok(cpy, " ");

        filename = strtok(NULL, " ");

        endtime_day = strtok(NULL, " ");
        endtime_time = strtok(NULL, " ");
        sprintf(endtime, "%s %s", endtime_day, endtime_time);
        
        if((strstr(cmd, "DELETE") != NULL) || (strstr(cmd, "delete") != NULL)) {
            cmd_delete(filename, endtime, buf);
        }

        else if((strstr(cmd, "RECOVER") != NULL) || (strstr(cmd, "recover") != NULL)) {
            cmd_recover(filename, buf);
        }

        else if((strstr(cmd, "TREE") != NULL) || (strstr(cmd, "tree") != NULL)) {
            cmd_tree();
        }

        else if((strstr(cmd, "EXIT") != NULL) || (strstr(cmd, "exit") != NULL)) {
            printf("프로그램 종료\n");

            gettimeofday(&end_t, NULL);
            ssu_runtime(&begin_t, &end_t);

            exit(0);
        }

        else if((strstr(cmd, "HELP") != NULL) || (strstr(cmd, "help") != NULL)) {
            cmd_help();
        }

        else if(strcmp(cmd, "\n") == 0)
            continue;

        else
            cmd_help();
    }

    exit(0);
}


/**
함 수 : cmd_delete(char *filename, char endtime[], char buf[])
기 능 : 프롬프트에서 입력받은 파일이 지정한 디렉토리에 존재하는지 확인한 후,
        endtime을 입력받았는지의 유무에 따라 trash 디렉토리를 생성하고, 삭제 동작을 수행하는 함수 호출
*/
void cmd_delete(char *filename, char endtime[], char buf[]) {

    char oldpath[MAX];
    char newpath[MAX];
    char timebuf[MAX];
    char yes_or_no[MAX]={0,};
    char ssu_mntr_path[MAX];
    char *ptr;
    
    pid_t pid;

    struct tm *date;
    time_t t;
    
    if(filename == NULL) {
        printf("FILENAME의 입력이 없습니다.\n");
        return;
    }
  
    if(strchr(filename, '/') != NULL) {
        filename = strrchr(filename, '/') + 1;
    }
 
    filename = remove_space(filename);
    
    getcwd(ssu_mntr_path, sizeof(ssu_mntr_path));

    if (my_scandir("check", 0, filename) == 0) {
        printf("해당 파일이 디렉토리에 존재하지 않습니다.\n");
        return;
    }
  
    chdir(ssu_mntr_path);
    

    if((ptr = strstr(buf, "ssu_mntr")) != NULL) //filename에 ssu_mntr이 포함된 경우
        ptr = ptr + 15;
    
    else {
        if((ptr = strstr(buf, "check")) != NULL) //filename에 check가 포함된 경우
            ptr = ptr + 6;
        else { //filename에 바로 파일이름이 나오는 경우
            if( ((ptr = strstr(buf, "DELETE")) != NULL) || ((ptr = strstr(buf, "delete")) != NULL) )
                ptr = ptr + 7;
        }
    }

    sprintf(oldpath, "%s", ptr);
    sprintf(oldpath, "%s%s", "check/", ptr);
    sprintf(newpath, "%s%s", "trash/files/", filename);


    /* -i 옵션 */
    if(strstr(buf, "-i") != NULL) {
        char *cpy_oldpath = malloc(strlen(oldpath)+1);
        strcpy(cpy_oldpath, oldpath);

        char* direct_oldpath = strtok(cpy_oldpath, " ");
        
        if(unlink(direct_oldpath) < 0) {
            perror("unlink error\n");
            exit(1);
        }
        return;
    }

    
      /* endtime이 지정된 경우 */
    if(strstr(endtime, "null") == NULL) {
        if(access("trash", F_OK) == 0) { //trash 디렉토리 존재
            if ((pid = fork()) < 0) {
                fprintf(stderr, "fork error\n");
                exit(1);
            }
            else if (pid == 0) { //자식
                while(1) {
                    time(&t);
                    date = localtime(&t);

                    memset(timebuf, 0, sizeof(timebuf));
                    sprintf(timebuf, "%04d-%02d-%02d %02d:%02d", date->tm_year+1900, date->tm_mon+1, date->tm_mday, date->tm_hour, date->tm_min);

                    /* endtime 에러처리 */
                    if(strcmp(endtime, timebuf) == -1) {
                        fprintf(stderr, "잘못된 endtime입니다.\n");
                        exit(1);
                    }
             
                    if(strstr(endtime, timebuf) != NULL) {
                        char *cpy_oldpath = malloc(strlen(oldpath)+1);
                        strcpy(cpy_oldpath, oldpath);

                        char* direct_oldpath = strtok(cpy_oldpath, " ");
                        sprintf(oldpath, "%s", direct_oldpath);

                        delete_action(filename, oldpath, newpath, buf);
                        exit(0);
                    }
                }
            }
            else //부모
                ;
        }

        else {
            if(mkdir("trash", 0766) == -1) {
                fprintf(stderr, "directory create error\n");
                exit(1);
            }
            if(mkdir("trash/files", 0766) == -1) {
                fprintf(stderr, "directory create error\n");
                exit(1);
            }
            if(mkdir("trash/info", 0766) == -1) {
                fprintf(stderr, "directory create error\n");
                exit(1);
            }


            if ((pid = fork()) < 0) {
                fprintf(stderr, "fork error\n");
                exit(1);
            }
            else if (pid == 0) { //자식
                while(1) {
                    time(&t);
                    date = localtime(&t);

                    memset(timebuf, 0, sizeof(timebuf));
                    sprintf(timebuf, "%04d-%02d-%02d %02d:%02d", date->tm_year+1900, date->tm_mon+1, date->tm_mday, date->tm_hour, date->tm_min);
        
                    /* endtime 에러처리 */
                    if(strcmp(endtime, timebuf) == -1) {
                        fprintf(stderr, "잘못된 endtime입니다.\n");
                        exit(1);
                    }

                    if(strstr(endtime, timebuf) != NULL) {
                        delete_action(filename, oldpath, newpath, buf);
                        exit(0);
                    }
                }
            }
            else //부모
                ;
        }
    }
    
 
    /* endtime이 지정되지 않은 경우 */
    if(strstr(endtime, "null") != NULL) {
        if(access("trash", F_OK) == 0) { //trash 디렉토리 존재
            delete_action(filename, oldpath, newpath, buf);
        }

        else {
            if(mkdir("trash", 0766) == -1) {
                fprintf(stderr, "directory create error\n");
                exit(1);
            }
            if(mkdir("trash/files", 0766) == -1) {
                fprintf(stderr, "directory create error\n");
                exit(1);
            }
            if(mkdir("trash/info", 0766) == -1) {
                fprintf(stderr, "directory create error\n");
                exit(1);
            }

            delete_action(filename, oldpath, newpath, buf);
        }
    }
}


/**
함 수 : cmd_recover(char *filename, char buf[])
기 능 : 프롬프트에서 입력받은 파일이 trash 디렉토리에 존재하는지 확인한 후,
        파일을 원래 경로로 복구하고 info 파일 삭제. 단, trash 디렉토리에 동일한 파일명이 존재하는 경우에는 파일이름과 삭제시간, 수정시간을
        표준출력으로 출력해 삭제할 파일을 고르도록하고, 파일을 복구할 때 원래의 경로에 동일한 파일이 있는 경우에는 파일이름 앞에 숫자를 추가하여 표시
*/
void cmd_recover(char *filename, char buf[]) {
    
    char ssu_mntr_path[MAX];
    char trash_path[MAX];
    char info_path[MAX];
    char info_absolute_path[MAX];
    char oldpath[MAX];
    char newpath[MAX];
    char cpy_newpath[MAX] = {0,};
    char trash_info_path[MAX] = {0,};

    char overlap_file[MAX][MAX];
    char recover_num;
    int num = 0;
    char identifier[MAX] = "1_";
    char char_idenfier[MAX] = "@";
    char filename_2[MAX];
    char alias_filename[MAX];

    int cnt = 0;
    int j=0;
    int n=0;
    int total_entry=0;
    
    struct dirent **items;
    int nitems;
    struct dirent **items_2;
    int nitems_2;

    FILE *fp;

    getcwd(ssu_mntr_path, sizeof(ssu_mntr_path));
    sprintf(trash_path, "%s%s", ssu_mntr_path, "/trash/files");
   
    filename = remove_space(filename);
    sprintf(trash_info_path, "%s%s", "trash/info/", filename);

    if (my_scandir(trash_path, 0, filename) == 0) {
        printf("There is no '%s' in the 'trash' directory\n", filename);
        return;
    }

    chdir(ssu_mntr_path);

    if((fp = fopen(trash_info_path, "r")) == NULL) { //info에서 정보 가져옴
        perror("fopen error\n");
        exit(1);
    }

    while(fgets(overlap_file[j], MAX, fp) != NULL) {
        overlap_file[j][strlen(overlap_file[j]) - 1] = '\0'; //개행문자 제거
        j++;
    }

    for(int i=0; i<MAX; i++) {
        if (access(overlap_file[3*(i+1)-2], F_OK) == 0)
            continue;
        else {
            sprintf(oldpath, "%s", overlap_file[3*(i+1)-2]);
            break;
        }
    }

    fclose(fp);


    /* 동일한 이름의 파일이 "trash" 디렉토리에 있는 경우, 파일 정보 표준출력 */
    if(chdir(trash_path) < 0) { //인자로 받은 디렉토리로 이동
        printf("DIR : %s\n", trash_path);
        perror("chdir ");
        exit(1);
    }

    nitems = scandir(".", &items, NULL, alphasort); //현재 디렉토리의 모든 파일과 디렉토리 내용 가져옴
    chdir(ssu_mntr_path);


    /* trash 디렉토리에 중복 파일이 있는지 검사 */
    for(int i=0; i<nitems; i++) {
        if((strstr(items[i]->d_name, filename)) != NULL) {
            strcpy(alias_filename, items[i]->d_name);
            cnt++;
        }
    }

    sprintf(newpath, "%s%s", "trash/files/", alias_filename);
    strcpy(cpy_newpath, newpath);

    /* trash 디렉토리에 중복 파일이 없는 경우 */
    if(cnt == 1) {
        /* 파일 복구 */
        sprintf(oldpath, "%s", overlap_file[1]);

        /* 복구 시 이름이 중복되는 경우 */
        if (access(oldpath, F_OK) == 0) {
            strcat(identifier, filename); //파일 처음에 숫자 추가
            strcpy(filename_2, identifier);
            sprintf(oldpath, "%s%s", "check/", filename_2);
        }

        chdir(ssu_mntr_path);
        
        if(rename(cpy_newpath, oldpath) == -1) { //파일 원래 경로로 복구
            fprintf(stderr, "rename error\n");
            exit(1);
        }

        sprintf(info_path, "%s%s", "trash/info/", filename);

        if(realpath(info_path, info_absolute_path) == NULL) {
            perror("realpath error\n");
            exit(1);
        }

        if(unlink(info_absolute_path) < 0) {
            fprintf(stderr, "unlink error for %s\n", info_absolute_path);
            exit(1);
        }
    }

    /* trash 디렉토리에 중복 파일이 있는 경우 */
    else {
        FILE *fp2; 

        j=0;
        n=0;  

        sprintf(trash_info_path, "%s%s", "trash/info/", filename);

        if((fp2 = fopen(trash_info_path, "r")) == NULL) { //info에서 정보 가져옴
            fprintf(stderr, "fopen error\n");
            exit(1);
        }

        while(fgets(overlap_file[j], MAX, fp2) != NULL) {
            overlap_file[j][strlen(overlap_file[j]) - 1] = '\0'; //개행문자 제거
            j++;
        }

        total_entry = j;


        for(int i=0; i<(total_entry-1)/3; i++) {
            n = i+1;
            printf("%d. %s  %s %s\n", n, filename, overlap_file[3*n-1], overlap_file[3*n]);
        }

        fclose(fp2);

        printf("Choose : ");
        recover_num = getchar();
        getchar();

        num = recover_num - 48; //char->int 변환

        sprintf(oldpath, "%s", overlap_file[3*n-2]);
        sprintf(newpath, "%s%s", "trash/files/", filename);

        for(int i=0; i<num-1; i++)
            strcat(newpath, char_idenfier);

        strcpy(cpy_newpath, newpath);


        /* 파일 복구 */

        /* 복구 시 이름이 중복되는 경우 */
        if (access(oldpath, F_OK) == 0) {
            strcat(identifier, filename); //파일 처음에 숫자 추가
            strcpy(filename_2, identifier);
            sprintf(oldpath, "%s%s", "check/", filename_2);
        }

        chdir(ssu_mntr_path);

        if(rename(cpy_newpath, oldpath) == -1) { //파일 원래 경로로 복구
            fprintf(stderr, "rename error\n");
            exit(1);
        }    
    }
}


/**
함 수 : cmd_tree()
기 능 : 지정한 디렉토리의 파일 구조를 트리 형태로 출력
*/
void cmd_tree() {

    printf("checkㅡㅡ");
    extra_function_scandir("check", make_tree, 0);
    printf("\n");
}


/**
함 수 : cmd_help()
기 능 : 프롬프트에서 사용할 수 있는 명령어의 사용법을 출력
*/
void cmd_help() {
    printf("ssu_mntr 명령어 사용법\n");
    printf("\n");

    printf("DELETE [FILENAME][END_TIME][OPTION] : 지정한 삭제 시간에 자동으로 파일 삭제\n");
    printf("    옵션\n");
    printf("    -i : 파일 영구 삭제\n");
    printf("    -r : 지정한 시간에 삭제 시, 삭제 여부 재확인\n");
    printf("\n");

    printf("SIZE [FILENAME][OPTION] : 파일경로(상대경로), 파일 크기 출력\n");
    printf("    옵션\n");
    printf("    -d <NUMBER> : NUMBER 단계만큼의 하위 디렉토리까지 출력\n");
    printf("\n");

    printf("RECOVER [FILENAME][OPTION] : trash 디렉토리 안에 있는 파일 원래 경로로 복구\n");
    printf("    옵션\n");
    printf("    -l : trash 디렉토리 밑에 있는 파일 목록 출력\n");
    printf("\n");

    printf("TREE : 모니터링 할 디렉토리 구조를 tree 형태로 출력\n");
    printf("\n");
    printf("EXIT : 프로그램 종료\n");
    printf("\n");
    printf("HELP : 명령어 사용법 출력\n");
}


/**
함 수 : delete_action(char *filename, char oldpath[], char newpath[], char buf[])
기 능 : 실질적으로 파일 삭제를 수행, 파일을 trash 디렉터리로 옮기고 info 파일에 지운 파일의 경로,
        삭제시간, 수정시간을 작성. info 파일의 용량이 2KB를 넘었을 경우, 오래된 파일의 순서대로
        삭제하도록 처리, trash 디렉토리에 동일한 파일의 이름이 있을 경우 이름 뒤에 특정 문자 붙어서 파일 이동
*/
void delete_action(char *filename, char oldpath[], char newpath[], char buf[]) {
    
    char *cpfilename = malloc(sizeof(char) * MAX);
    char trash_info_path[MAX] = {0,};
    char cpy_newpath[MAX] = {0,};
    char absolute_path[BUF_SIZE] = {0,};
    char new_absolute_path[BUF_SIZE] = {0,};
    off_t infofile_size = 0;
    char ssu_mntr_path[MAX];
    char trash_path[MAX];
    int existance = 0;

    struct stat statbuf_file;
    struct stat trash_statbuf_file;

    strcpy(cpy_newpath, newpath);

    oldpath = remove_space(oldpath);
    
    if(realpath(oldpath, absolute_path) == NULL) {
        perror("realpath error\n");
        exit(1);
    }

    memset(&statbuf_file, 0, sizeof(statbuf_file));
    lstat(absolute_path, &statbuf_file);


    getcwd(ssu_mntr_path, sizeof(ssu_mntr_path));
    sprintf(trash_path, "%s%s", ssu_mntr_path, "/trash/files");
  
    /* "trash" 디렉토리에 동일한 파일이 있는 경우, 파일 구분 위해 이름 변경 */
    while(my_scandir(trash_path, 0, filename)) {
        strcat(cpy_newpath, "@");
        strcat(filename, "@");
        existance = 1;
    }
  
    chdir(ssu_mntr_path);
 

    /* 파일 삭제 */
    if(rename(oldpath, cpy_newpath) == -1) { //파일 trash 디렉토리로 이동
        fprintf(stderr, "rename error\n");
        exit(1);
    }

    if(realpath(cpy_newpath, new_absolute_path) == NULL) {
        perror("realpath error\n");
        exit(1);
    }
   
    if (existance == 1) //"trash" 디렉토리에 동일한 파일이 있는 경우
        eliminate_certain_character(filename, '@');

    chdir(ssu_mntr_path);

    memset(&trash_statbuf_file, 0, sizeof(trash_statbuf_file));
    lstat(new_absolute_path, &trash_statbuf_file);

    sprintf(trash_info_path, "%s%s", "trash/info/", filename);


    if (existance == 1) { //"trash" 디렉토리에 동일한 파일이 있는 경우, info 파일에 내용 추가
        
        FILE *fp;

        chdir(ssu_mntr_path);

        if((fp = fopen(trash_info_path, "a+")) == NULL) { //info 디렉토리에 파일 생성
            perror("fopen error\n");
            exit(1);
        }

        fprintf(fp, "%s\n", absolute_path); //명령어로 지운 파일 절대경로
        fprintf(fp, "D : %s\n", print_time(localtime(&trash_statbuf_file.st_ctime))); //삭제 시간
        fprintf(fp, "M : %s\n", print_time(localtime(&statbuf_file.st_mtime))); //최종 수정 시간

        fclose(fp);
    }

    else {
        FILE *fp;

        if((fp = fopen(trash_info_path, "w+")) == NULL) { //info 디렉토리에 파일 생성
            perror("fopen error\n");
            exit(1);
        }

        fputs("[Trash info]\n", fp);
        fprintf(fp, "%s\n", absolute_path); //명령어로 지운 파일 절대경로
        fprintf(fp, "D : %s\n", print_time(localtime(&trash_statbuf_file.st_ctime))); //삭제 시간
        fprintf(fp, "M : %s\n", print_time(localtime(&statbuf_file.st_mtime))); //최종 수정 시간

        fclose(fp);

    }


    /* info 파일 용량 초과 */
    while(1) {
        infofile_size = calculate_infofile_size();

        if(infofile_size > 2000)
            delete_oldfile(infofile_size);
        else
            break;
    }
}


/**
함 수 : my_scandir(char *wd, int depth, char *filename)
기 능 : 인자로 받은 디렉토리에 있는 모든 파일과 하부 디렉토리 파일 검색
*/
int my_scandir(char *wd, int depth, char *filename) {
    struct dirent **items;
    int nitems;
    char* file;

    if(chdir(wd) < 0) { //인자로 받은 디렉토리로 이동
        printf("DIR : %s\n", wd);
        perror("chdir ");
        exit(1);
    }

    nitems = scandir(".", &items, NULL, alphasort); //현재 디렉토리의 모든 파일과 디렉토리 내용 가져옴


    /* 디렉토리 항목 갯수만큼 루프 돌리면서 해당 파일이 디렉토리일 경우, my_scandir 함수 재귀 호출 */
    for(int i=0; i<nitems; i++) {
        struct stat fstat; //파일 상태 저장하기 위한 구조체

        /* 현재 디렉토리, 이전 디렉토리 무시 */
        if((!strcmp(items[i]->d_name, ".")) || (!strcmp(items[i]->d_name, "..")))
            continue;

        file = items[i]->d_name;

        lstat(items[i]->d_name, &fstat);

        /* 파일이 디렉토리라면 my_scandir 함수 재귀 호출하고, depth 레벨을 1 증가시킴 */
        if((fstat.st_mode & S_IFDIR) == S_IFDIR) {

            /* depth만큼 하위 디렉토리 검색 (depth가 0일 경우 깊이에 관계없이 검색) */
            if(indent < (depth-1) || (depth == 0)) {
                indent++;

                if (my_scandir(items[i]->d_name, depth, filename) == 1) {
                    return 1;
                }
            }
        }

        else {
            if(strstr(file, filename) != NULL)
                return 1;
        }
    }
   
    indent--; //디렉토리의 depth 레벨 1 감소시킴
    chdir(".."); //하위 디렉토리로 이동

    return 0;
}


/**
함 수 : make_tree(char *file)
기 능 : 프롬프트에 tree 명령어를 입력했을 경우, 지정한 파일을 tree 구조로 출력
*/
void make_tree(char *file) {
    char file_absolute_path[MAX];
    int cnt = 0;
    
    if(realpath(file, file_absolute_path) == NULL) {
        perror("realpath error\n");
        exit(1);
    }

    /* tree를 그리기 위해 '/'개수 count */
    for(int i=0; file_absolute_path[i] != 0; i++) {
        if(file_absolute_path[i] == '/')
            cnt++;
    }

    if(cnt == 5)
        printf("ㅡㅡㅡ");

    else if(cnt == 6)
        printf("  |ㅡ");

    else {
        for(int i=0; i<cnt-6; i++)
            printf("  |");
        printf("  |ㅡ");
    }



    printf("%s\n", file);

    if(cnt == 5)
        printf("        |");
    else
        printf("        |");
}


/**
함 수 : extra_function_scandir(char *wd, void (*func)(char *), int depth)
기 능 : 인자로 받은 디렉토리에 있는 모든 파일과 하부 디렉토리 파일 검색, 각각의 파일에 대한 함수 실행
*/
void extra_function_scandir(char *wd, void (*func)(char *), int depth) {
    struct dirent **items;
    int nitems;

    if(chdir(wd) < 0) { //인자로 받은 디렉토리로 이동
        printf("DIR : %s\n", wd);
        perror("chdir ");
        exit(1);
    }

    nitems = scandir(".", &items, NULL, alphasort); //현재 디렉토리의 모든 파일과 디렉토리 내용 가져옴


    /* 디렉토리 항목 갯수만큼 루프 돌리면서 해당 파일이 디렉토리일 경우, extra_function_scandir 함수 재귀 호출 */
    for(int i=0; i<nitems; i++) {
        struct stat fstat; //파일 상태 저장하기 위한 구조체

        /* 현재 디렉토리, 이전 디렉토리 무시 */
        if((!strcmp(items[i]->d_name, ".")) || (!strcmp(items[i]->d_name, "..")))
            continue;

        func(items[i]->d_name);

        lstat(items[i]->d_name, &fstat);

        /* 파일이 디렉토리라면 extra_function_scandir 함수 재귀 호출하고, depth 레벨을 1 증가시킴 */
        if((fstat.st_mode & S_IFDIR) == S_IFDIR) {

            /* depth만큼 하위 디렉토리 검색 (depth가 0일 경우 깊이에 관계없이 검색) */
            if(indent < (depth-1) || (depth == 0)) {
                indent++;

                extra_function_scandir(items[i]->d_name, func, depth);
            }
        }
    }
   
    indent--; //디렉토리의 depth 레벨 1 감소시킴
    chdir(".."); //하위 디렉토리로 이동
}


/**
함 수 : calculate_infofile_size()
기 능 : info 파일 크기 계산
*/
int calculate_infofile_size() {
    char info_absolute_path[BUF_SIZE] = {0,};
    char dname_path[MAX] = {0,};
    char dname_absolute_path[BUF_SIZE] = {0,};
    off_t infofile_size = 0;
    
    struct stat statbuf_info;
    struct dirent *dentry = NULL;
    char filename[MAX];
    DIR *dirp = NULL;
    
    if(realpath("trash/info", info_absolute_path) == NULL) {
        perror("realpath error\n");
        exit(1);
    }

    if ((dirp = opendir(info_absolute_path)) == NULL) {
        fprintf(stderr, "opendir error for %s\n", info_absolute_path);
        exit(1);
    }
    
    while ((dentry = readdir(dirp)) != NULL) {
   
        if((!strcmp(dentry->d_name, ".")) || (!strcmp(dentry->d_name, "..")))
            continue;
        
        sprintf(dname_path, "%s%s", "trash/info/", dentry->d_name);

        if(realpath(dname_path, dname_absolute_path) == NULL) {
            perror("realpath error\n");
            exit(1);
        }
        stat(dname_absolute_path, &statbuf_info);
        infofile_size += statbuf_info.st_size;
    }

    closedir(dirp);
   
    return infofile_size;
}


/**
함 수 : delete_oldfile(off_t infofile_size)
기 능 : info 파일에 저장된 삭제시간을 기준으로 파일들을 비교하여 삭제 시간이 가장 오래된 파일을 찾고,
        해당 파일 삭제
*/
void delete_oldfile(off_t infofile_size) {

    char info_absolute_path[BUF_SIZE] = {0,};
    char dname_path[MAX] = {0,};
    char dname_absolute_path[BUF_SIZE] = {0,};
    char tmp[MAX] = {0,};
    char tmp_time[MAX] = {0,};
    char file[MAX] = {0,};
    char file_time[MAX] = {0,};
    char trash[MAX] = {0,};
    char *delete_file;
    char delete_file_path[BUF_SIZE] = {0,};
    char ssu_mntr_path[MAX];
   
    struct stat statbuf_info_file;
    struct dirent *dentry = NULL;
    char filename[MAX];
    DIR *dirp = NULL;

    FILE *fp;

    struct dirent **items;
    int nitems;

    
    if(realpath("trash/info", info_absolute_path) == NULL) {
        perror("realpath error\n");
        exit(1);
    }

    if ((dirp = opendir(info_absolute_path)) == NULL) {
        fprintf(stderr, "opendir error for %s\n", info_absolute_path);
        exit(1);
    }
    

    for (int i=0;;i++) {
        if((dentry = readdir(dirp)) == NULL)
            break;
   
        if((!strcmp(dentry->d_name, ".")) || (!strcmp(dentry->d_name, "..")))
            continue;
        
        sprintf(dname_path, "%s%s", "trash/info/", dentry->d_name);

        if(realpath(dname_path, dname_absolute_path) == NULL) {
            perror("realpath error\n");
            exit(1);
        }

        if((fp = fopen(dname_absolute_path, "r")) == NULL) { //info에서 정보 가져옴
            fprintf(stderr, "fopen error\n");
            exit(1);
        }

        fgets(trash, sizeof(trash), fp);
        fgets(trash, sizeof(trash), fp);
        

        if(i==0) {
            strcpy(tmp, dname_absolute_path);
            fgets(tmp_time, sizeof(tmp_time), fp);
        }
        else {
            strcpy(file, dname_absolute_path);
            fgets(file_time, sizeof(file_time), fp);
        }


        if(strcmp(tmp_time, file_time) == 1) {
            memset(&tmp, 0, sizeof(tmp));
            memset(&tmp_time, 0, sizeof(tmp_time));

            strcpy(tmp, file);
            strcpy(tmp_time, file_time);
        }
    }
        
    fclose(fp);
    closedir(dirp);

    /* info 디렉토리 파일 정보 삭제 */
    if(unlink(tmp) < 0) {
        fprintf(stderr, "unlink error for %s\n", tmp);
        exit(1);
    }

    delete_file = strrchr(tmp, '/') + 1;
    
    
    getcwd(ssu_mntr_path, sizeof(ssu_mntr_path));

    if(chdir("trash/files") < 0) { //인자로 받은 디렉토리로 이동
        printf("DIR : %s\n", "trash/files");
        perror("chdir ");
        exit(1);
    }

    nitems = scandir(".", &items, NULL, alphasort); //현재 디렉토리의 모든 파일과 디렉토리 내용 가져옴
    chdir(ssu_mntr_path);


    /* files 디렉토리 원본파일 삭제 (중복파일 포함) */
    for(int i=0; i<nitems; i++) {
        if((strstr(items[i]->d_name, delete_file)) != NULL) {
            sprintf(delete_file_path, "%s%s", "trash/files/", items[i]->d_name);

            if(unlink(delete_file_path) < 0) {
                fprintf(stderr, "unlink error %s\n", delete_file_path);
                exit(1);
            }
        }
    }
}


/**
함 수 : make_original_list(char *file)
기 능 : 재귀적으로 호출되며 인자로 넘기는 file 목록과 파일의 수정시간을 original list에 저장
*/
void make_original_list(char *file) {
    char ssu_mntr_path[MAX];
    char tmp[MAX];
    char *ptr;

    struct stat statbuf_original;

    getcwd(ssu_mntr_path, sizeof(ssu_mntr_path));
    
    sprintf(original_path, "%s/%s", ssu_mntr_path, file);
    lstat(original_path, &statbuf_original);  

    sprintf(original_time[original_list_cnt], "%s", print_time(localtime(&statbuf_original.st_mtime)));


    sprintf(tmp, "%s_%s", ssu_mntr_path, file);

    ptr = strstr(tmp, "check");
    ptr = ptr + 6;
    
    sprintf(original_list[original_list_cnt], "%s", ptr);

    original_list_cnt++;
}


/**
함 수 : make_new_list(char *file)
기 능 : 재귀적으로 호출되며 인자로 넘기는 file 목록과 파일의 수정시간을 new list에 저장
*/
void make_new_list(char *file) {
    char ssu_mntr_path[MAX];
    char tmp[MAX];
    char *ptr;

    struct stat statbuf_new;

    getcwd(ssu_mntr_path, sizeof(ssu_mntr_path));
    
    sprintf(new_path, "%s/%s", ssu_mntr_path, file);
    lstat(new_path, &statbuf_new);   

    sprintf(new_time[new_list_cnt], "%s", print_time(localtime(&statbuf_new.st_mtime)));


    sprintf(tmp, "%s_%s", ssu_mntr_path, file);

    ptr = strstr(tmp, "check");
    ptr = ptr + 6;
    
    sprintf(new_list[new_list_cnt], "%s", ptr);
    
    new_list_cnt++;
}


/**
함 수 : remove_space(char *str)
기 능 : 인자로 넘긴 문자열에서 "\r", "\n", "\t" 제거
*/
char* remove_space(char *str) {
    char buf[BUF_SIZE] = {0,};
    char *p;
    
    p = strtok(str, "\r\n\t");
    strcat(buf, p);

    while (p != NULL) {
        p = strtok(NULL, "\r\n\t");
        
        if(p != NULL) {
            strcat(buf, " ");
            strcat(buf, p);
        }
    }
    memset(str, '\0', BUF_SIZE);
    strcpy(str, buf);

    return str;
}


/**
함 수 : print_time(struct tm* t)
기 능 : 인자로 받은 time 구조체를 주어진 출력 형태에 맞게 변경하고 해당 문자열을 리턴
*/
char* print_time(struct tm* t) {
    char *timebuf;
    
    memset(timebuf, 0, sizeof(timebuf));
    sprintf(timebuf, "%04d-%02d-%02d %02d:%02d:%02d", t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

    return timebuf;
}


/**
함 수 : eliminate_certain_character(char *str, char ch)
기 능 : 문자열에서 인자로 받은 특정 문자를 제거
*/
void eliminate_certain_character(char *str, char ch) {

    for(; *str != '\0'; str++) {
        if(*str == ch) {
            strcpy(str, str+1);
            str--;
        }
    }
}


/**
함 수 : ssu_daemon(void)
기 능 : 데몬 프로세스를 생성하고, 무한 루프로 파일의 생성, 삭제, 수정 여부를 확인하여 log 찍음
*/
int ssu_daemon(void) {
    pid_t pid;
    int fd, maxfd;
    

    if ((pid = fork()) < 0) {
        fprintf(stderr, "fork error\n");
        exit(1);
    }
    else if (pid != 0)
        exit(0);

    pid = getpid();
    setsid();
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    maxfd = getdtablesize();

    for (fd=0; fd<maxfd; fd++)
        close(fd);

    umask(0);
    dup(0);
    dup(0);


     FILE *fp;

     int k=0;
     
     if((fp = fopen("log.txt", "a+")) == NULL) { //log.txt 파일 생성
        perror("fopen error\n");
        exit(1);
    }

    chmod("log.txt", 777);
    

    /* 파일 변경상태 모니터링 */
    while(1) {

        if(k == 0)
            extra_function_scandir("check", make_original_list, 0);
        
        sleep(1);

        extra_function_scandir("check", make_new_list, 0);


        /* 파일 변경 상태 검사 */
        int mark[MAX];
        struct tm *date;
        time_t t;

        for(int i=0; i<MAX; i++)
            mark[i] = 1;


        /* 파일이 삭제된 경우 */
        for(int i=0; i<original_list_cnt; i++) {
            for(int j=0; j<new_list_cnt; j++) {
                if(strcmp(original_list[i], new_list[j]) == 0)
                    mark[i] = 0; //new list에 파일이 계속 있는 경우
            }
        }

        for(int i=0; i<original_list_cnt; i++) {
            if (mark[i] == 1) {
                time(&t);
                date = localtime(&t);

                fprintf(fp, "[%s][delete_%s]\n", print_time(date), original_list[i]);
                fflush(fp);
            }
        }

        for(int i=0; i<MAX; i++)
            mark[i] = 1;


        /* 파일이 생성된 경우 */
        for(int i=0; i<new_list_cnt; i++) {
            for(int j=0; j<original_list_cnt; j++) {
                if(strcmp(new_list[i], original_list[j]) == 0)
                    mark[i] = 0; //original list에 파일이 원래 있는 경우
            }
        }

        for(int i=0; i<new_list_cnt; i++) {
            if (mark[i] == 1) {
                time(&t);
                date = localtime(&t);

                fprintf(fp, "[%s][create_%s]\n", print_time(date), new_list[i]);
                fflush(fp);
            }
        }


        /* 파일이 수정된 경우 */
        for(int i=0; i<original_list_cnt; i++) {
            for(int j=0; j<new_list_cnt; j++) {
                if(strcmp(original_list[i], new_list[j]) == 0) { //original_list에 있는 파일이 new_list에도 존재하는 경우

                

                    if(strcmp(original_time[i], new_time[j]) != 0) { //두 파일의 수정 시간이 다른 경우
                        time(&t);
                        date = localtime(&t);

                        fprintf(fp, "[%s][modify_%s]\n", print_time(date), original_list[i]);
                        fflush(fp);
                    }
                }
            }
        }


        memset(original_list, 0, MAX*MAX);

        for(int i=0; i<MAX; i++)
            strcpy(original_list[i], new_list[i]);

        memset(new_list, 0, MAX*MAX);


        original_list_cnt = new_list_cnt;
        new_list_cnt = 0;


        memset(original_time, 0, MAX*MAX);

        for(int i=0; i<MAX; i++)
            strcpy(original_time[i], new_time[i]);

        memset(new_time, 0, MAX*MAX);


        k = 1;
    }

    fclose(fp);
    
    return 0;
}


/**
함 수 : ssu_runtime(struct timeval *begin_t, struct timeval *end_t)
기 능 : ssu_mntr의 수행 시간을 측정
*/
void ssu_runtime(struct timeval *begin_t, struct timeval *end_t) {
    
    end_t->tv_sec -= begin_t->tv_sec;

    if(end_t->tv_usec < begin_t->tv_usec) {
        end_t->tv_sec--;
        end_t->tv_usec += SECOND_TO_MICRO;
    }

    end_t->tv_usec -= begin_t->tv_usec;
    printf("Runtime: %ld:%06ld(sec:usec)\n", end_t->tv_sec, end_t->tv_usec);
}