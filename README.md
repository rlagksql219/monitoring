# 🔍 프로젝트 개요

> 지정한 디렉토리를 모니터링하며 변경사항을 로그로 남기는 프로그램입니다.


## 목표

리눅스 컴퓨팅 환경에서 프로세스가 동작하는 과정을 이해하고, 파일속성 디렉토리에 관한 정보를 비교하여 프로그램을 작성해봄으로써 시스템 프로그램 설계 및 응용 능력을 향상시키기 위해 프로그램을 개발하게 되었습니다.


## 프로그램 기본 사항

- **프로그램 실행 시, 명령어를 수행 할 프롬프트 출력**
    - DELETE: 지정한 삭제 시간에 자동으로 파일을 삭제해주는 명령어
    - RECOVER: 삭제한 파일을 원래 경로로 복구하는 명령어
    - TREE: 모니터링 중인 디렉토리를 tree 형태로 보여주는 명령어
    - EXIT: 프로그램을 종료시키는 명령어
    - HELP: 명령어 사용법을 출력하는 명령어

- **파일이 생성, 삭제, 수정될 경우, `log.txt` 파일에 변경 사항 추가**

- **모니터링을 담당하는 프로세스 생성**
<br/>


# 🔍 설계

프로그램은 프롬프트에서 주어진 명령어를 수행하는 프로세스와 지정한 디렉토리의 파일 변경 상태를 모니터링하는 프로세스로 나뉘어지고, 모니터링 프로그램은 디몬 프로세스로 구현하였습니다. 


## 함수 간 Call Graph
![제목 없음](https://user-images.githubusercontent.com/69866091/151962868-cf01e944-a0f5-4dc7-93d0-222db2a7a5ae.png)

  
## 함수 기능별 흐름도
![main](https://user-images.githubusercontent.com/69866091/152007369-565eff88-a5f1-4235-8c7f-08be58de03e8.png)

- int ssu_daemon(void); <br/>
데몬 프로세스를 생성하여 파일의 삭제, 생성, 수정을 모니터링하며 log를 남기는 함수

![ssu_daemon](https://user-images.githubusercontent.com/69866091/152003543-1dae7f51-251a-48a3-948c-eecdb4f38523.png)

- void cmd_delete(char *filename, char endtime[ ], char buf[ ]); <br/>
프롬프트에서 입력받은 파일이 지정한 디렉토리에 존재하는지 확인한 후, endtime을 입력 받았는지의 유무에 따라 trash 디렉토리를 생성하고, 삭제 동작을 수행하는 함수

![cmd_delete](https://user-images.githubusercontent.com/69866091/152003550-34ae1976-6bcf-459c-a1a2-717c76275f87.png)

- void calculate_infofile_size(void); <br/>
info 파일의 크기를 계산하는 함수

![calculate](https://user-images.githubusercontent.com/69866091/152005981-58bb192c-8164-42df-81ce-d90c1d7a08ff.png)

- void delete_oldfile(off_t infofile_size); <br/>
info 파일에 저장된 삭제 시간을 기준으로 파일을 비교하여 삭제 시간이 가장 오래된 파일을 찾고, 해당 파일을 삭제하는 함수

![delete_oldfile](https://user-images.githubusercontent.com/69866091/152006085-8ae3c8cf-362e-4081-bf3f-548994d3e8f9.png)

- void cmd_recover(char *filename, char buf[ ]); <br/>
프롬프트에서 입력받은 파일이 trash 디렉토리에 존재하는지 확인 후, 파일을 원래 경로로 복구하고 info 파일을 삭제하는 함수

![recover](https://user-images.githubusercontent.com/69866091/152006231-aaa307a0-ac09-4709-b7e3-df02100d679b.png)
<br/>
<br/>


# 🥣 테스트 및 결과

> ✨[테스트 결과](https://www.notion.so/3c29003eee594a259a0e4712feb8ea95) 
