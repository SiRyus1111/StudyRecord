## 0. 서론

코드 설명을 하려고 했는데, 이거 지금까지 커밋을 아예 안해서ㅋㅋ 어떻게 해야할지 모르겠네..
그래서 코드를 짠 과정보다는 앞에서부터 쭉 읽어가면서
실행되는 순서대로 이 코드를 왜 이렇게 썼는지 등을 설명해볼게.

그리고 읽어보면서 잘못된 점이 있다면, 수정하기도 하면서 진행할게.

이번 글은 `헤더` / `라이브러리` / `전역 상수` / `상태 관리 구조체` 파트까지만..

[깃허브 링크](https://github.com/SiRyus1111/My-First-Echo-Server-Project)

그리고 아직 클라이언트쪽 코드는 거의 안 썼어. 
정확히는, 구판 코드야. 예전에 그냥 돌아가는 것만 보려고 패킷 헤더같은것도 안 붙인 코드.

그래서 이 코드들은 전부 서버 코드야.

## 1. 헤더 / 라이브러리
```cpp
#include <iostream> // 콘솔 입출력 용 - cout, cin, ...
#include <winsock2.h> // 윈속2 메인 헤더 - socket(), bind(), listen(), accept(), recv(), send(), ...
#include <ws2tcpip.h> // 윈속2 확장 헤더 - inet_ntop(), inet_pton(), ...
#include "Common.h" // 사용자 정의 라이브러리. 소켓 함수 오류 출력 함수 포함. err_quit(), err_display() 함수는 Common.h에 정의되어 있음.
// #include <cstdio> / 이거 왜 썼을까? / 일단 지금은 이 라이브러리가 있었다는 기록만 남겨둠. 주석 처리. 주석 처리.
#include <cstdlib> // atoi() 함수 사용하기 위해서
#include <cstring> // memcpy() 함수 사용하기 위해서
```

맨 위에는 당연히 전처리가 필요한 라이브러리들과,
앞으로도 쭉 쓸 상수 / 전역변수들이 들어가야겠지.

일단 `iostream`.
이건 중간에 서버 콘솔창에 클라이언트로부터 받은 메시지를 출력하는 부분. 
이 곳에서 쓰기 위해서 인클루드했어.

이제 `winsock2.h`.
이건 당연히 윈속 API들을 쓰기 위해서 인클루드했어.

`ws2tcpip.h`는
윈속 확장 헤더인데,
`inet_ntop()` / `inet_pton()` 을 쓸 수 있게 해주는 헤더야.
물론 IPv6 지원도 해주고.
그래서 접속한 클라이언트의 IP를 문자열로 바꿔서 출력하기 위해
`inet_ntop()`를 쓸 필요가 있어서 인클루드했어.

`Common.h`는 사용자 정의 라이브러리인데,
이건 그러니까 `WSAGetLastError()` 함수를 더 쉽게 사용할 수 있게,
특정 함수에 오류가 발생했을 때, 
해당 함수의 이름(예 : `socket()`)을 넣으면 자동으로 해당 함수의 이름 출력, 
그리고 `WSAGetLastError()`를 호출하고, 
그대로 실행하거나(`err_display()` 함수)
아예 종료하거나(`err_quit()` 함수)
해주는 함수들이 포함된 라이브러리야.
```cpp
#define _CRT_SECURE_NO_WARNINGS // 구형 C 함수 사용 시 경고 끄기
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 구형 소켓 API 사용 시 경고 끄기

#include <winsock2.h> // 윈속2 메인 헤더
#include <ws2tcpip.h> // 윈속2 확장 헤더

#include <tchar.h> // _T(), ...
#include <stdio.h> // printf(), ...
#include <stdlib.h> // exit(), ...
#include <string.h> // strncpy(), ...

#pragma comment(lib, "ws2_32") // ws2_32.lib 링크

// 소켓 함수 오류 출력 후 종료
void err_quit(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char *)&lpMsgBuf, 0, NULL);
	MessageBoxA(NULL, (const char *)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char *)&lpMsgBuf, 0, NULL);
	printf("[%s] %s\n", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// 소켓 함수 오류 출력
void err_display(int errcode)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char *)&lpMsgBuf, 0, NULL);
	printf("[오류] %s\n", (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}
```
이렇게 생겼어.
내가 직접 만든거는 아니고, 
`TCP / IP 소켓 프로그래밍(김선우 저) 책`에서 제공한 헤더를 쓰고있어.

이제 `cstdlib`.
이거는 중간에 헤더를 해석하는 부분에서,
그저 `char*` 형인 헤더를 다시 `uint32_t` 형으로 바꿔주는 `atoi()` 함수를 사용하기 위해서 인클루드했어.
**근데 이건 틀렸어..**
`atoi() - ASCII to integer` 함수는 그저 문자열을 숫자로 바꾸는 함수라서 
`uint32_t` 형의 바이트열을 그냥 바로 문자열로 해석해서 숫자로 바꿔버려.
`atoi()` 함수는 결국 `문자열 -> 숫자`로 바꾸는 함수니,
`바이트열 -> 숫자`로 바꿔야하는 목표와 충돌해.
이게 뭔 차이냐고 할 수 있지만,
```cpp
"1234" -> 1234
```
이렇게 바꾸는거면 그냥 `atoi()`를 써도 되지만,
실제로는
```cpp
[0x00, 0x00, 0x04, 0xD2] -> 1234
```
이런 작업을 해야해.
물론 바이트 정렬에 따라 데이터의 의미가 달라지기도 하니 문제가 될 수 있겠지.

그래서 나는 `cstring` 라이브러리의 `memcpy() - memory copy` 함수로 바이트를 그대로 복사해서 `uint32_t`형 변수에 넣은 후,
`ntohl()` 함수를 사용해서 다시 호스트 바이트 정렬로 바꿔주는 식으로 수정했어.

그러니까 
>문자를 숫자로 변환 vs 바이트를 그대로 복사

의 차이라는건데,
우리는 **네트워크 바이트 정렬**의 **바이트 열**을 숫자로 바꾸는 거니까 바이트를 그대로 복사해야한 후 호스트 바이트 정렬로 변환해야하는거야.

## 2. 전역 상수
```cpp
const int SERVER_PORT = 9000;
const int BUFFER_SIZE = 4096;
const int HEADER_SIZE = 4;
```
그 다음에는 상수로 
- 서버의 포트 번호(`SERVER_PORT`) - 9000번 포트 사용
- 송 / 수신할 메시지를 담을 버퍼의 최대 크기(`BUFFER_SIZE`) - 최대 4096바이트
- 고정 길이 헤더의 크기(`HEADER_SIZE`) - 4바이트(`uint32_t`)

이 세 값들은 앞으로도 `main()` 함수 뿐만이 아닌 
다른 함수, 다른 소스 코드에서도 쓸 일이 있을 가능성이 크다고 생각해서 전역 상수의 형태로 선언했어.

그리고 추가로 이 세 값들은 상수로 선언하기 때문에, 
딱히 값이 바뀌지 않아서 레이스 걱정을 안해도 돼.
그래서 안심하고 전역으로 선언했어.

왜 9000번 포트를 사용했냐면, 웰 노운 포트는 충돌 가능성 등 이런 간단한 프로젝트에 사용하기에는 수지타산이 맞지 않는다고 생각했어. 
서버-클라이언트간의 송수신 구조를 깊게 파기엔 오히려 웰 노운 포트 충돌 해결같은 문제에 너무 무게가 쏠릴까봐.

그리고 왜 4096바이트를 메시지 버퍼의 크기로 잡았냐면, 
그냥 어떻게 맞추지.. 하다가
일반적인 가상메모리 기법 중 하나인 페이징의 
페이지의 일반적인 크기(4KB)가 떠올라서
그냥 그렇게 하면 딱 한 페이지에 들어가지 않을까?
라는 야매적인 발상으로 4096바이트로 설정했어.

물론 확실히 MSS(일반적인 이더넷 환경에서는 최대 1460바이트인 경우 많음)를 넘도록해서 
버퍼의 최대 크기(4096바이트)로 메시지를 보내면 
송신할 때 웬만해서는 하나 이상의 세그먼트로 쪼개질 것 같아서야.
솔직히 이건 그냥 내 추측이기는 한데..
그렇게 바이트열이 쪼개지는걸 체감해보고 싶었어.

그리고 고정 길이 헤더의 크기는..
이건 왜 따로 상수로 뺐는지에 대한 얘기를 중심으로 해야할 것 같은데,
쉽게 말하면, 추가로 `uint32_t` 형의 길이를 나타내는 헤더 말고
나중에 추가로 다른걸 더 보내야할 수도 있잖아? 
예를 들어, 서버에서 오류가 났을 때, 그걸 클라이언트에 알리는 플래그라던지..
그래서 이거 지역 변수나 하드코딩으로 박아넣으면 
안될 것 같아서 전역 상수로 선언했어.

## 3. #pragma
```cpp
#pragma comment(lib, "Ws2_32.lib")
```

그리고 `#pragma comment(lib, "Ws2_32.lib")`은..
나 솔직히 이거 이해 안하고 그냥 복붙만 했어.
예전에 에코 서버 예제에 들어있었는데,
이거 빼면 무슨 일 터질까 무서워서..ㅋㅋ

그래서 이번 기회에 제대로 알아봤는데,
`#pragma`란, 문법 자체를 바꾸지는 않지만
쉽게 말하면 **컴파일러 / 링커가 제공하는 구현 별 기능을 ON / OFF** 해주는거야.

그리고 나는 **해당 코드를 컴파일할 때 컴파일러 / 링커에 추가 지시를 내려주는 문법**이라고 이해했어.
**빌드할 때의 동작을 좀 더 지시**한다고 해야할까?

그러니까 내가 쓴 
`#pragma comment(lib, "Ws2_32.lib")`은,
링크를 할 때, 링커에서
`Ws2_32.lib`이란 라이브러리(`socket()` / `bind()` 등이 있는 라이브러리)를 꼭 링크하라고
명시적으로 선언한거지.

그리고 이 `#pragma comment` 는 `MSVC` 전용이라는 성격이 강해서,
이식성은 떨어진대. 
물론 컴파일러(`gcc`, `clang` 등)마다 지원하는 `#pragma`도 다르니까,
항상 `#pragma`를 쓸 때는 해당 컴파일러가 그 `#pragma`를 지원하는지 잘 알아봐야 할 것 같아.

하지만 굳이 `winsock2.h` 헤더를 붙였는데 이것까지 명시할 이유가 있을까 싶어서 찾아보니까,
헤더는 그저 함수의 이름표.
이 함수는 뭘 인자로 받고, 뭘 반환하고..
이런 것들이 적혀있고,
그 함수의 실제 구현은 라이브러리에 있다고 하더라고..

이름표만 있고 실제 구현이 없으면 확실히 문제가 생기겠지.
그리고 `socket()` 함수같은 `Ws2_32.lib` 라이브러리가 필요한 함수가 있더라도
링커가 `winsock` 라이브러리를 붙여야겠다고 추론해주진 않아.

그래서 이렇게 의존성이 있는 라이브러리를 컴파일러 / 링커에게 알려주기 위해서 
코드에 직접 이렇게 명시하는 방식 
```cpp
#pragma comment(lib, "Ws2_32.lib")
```
을 썼다고 보면 돼.

너무 딴 길로 샌 것 같은데, 결국
> 헤더 파일은 함수와 타입의 선언을 제공해서 컴파일러가 코드를 해석할 수 있게 해주고, 
라이브러리는 그 함수들의 실제 구현을 제공해서 
링크 단계에서 최종 실행 파일을 만들 수 있게 해준다.

라는거야.

이건 나중에 좀 더 파볼게. 이 글에서는 지금도 너무 길다.

## 4. 상태 관리 구조체

저번글에 있던 대로야.
```cpp
struct flags {

	bool header_recv = false;
	bool payload_recv = false;
	bool header_send = false;
	bool payload_send = false;

	bool if_error = false;
	bool if_client_exit = false;
    
};
```
현재 상태
- `header_recv` : 헤더 `recv()` 하는 중
- `payload_recv` : 페이로드 `recv()` 하는 중
- `header_send` : 헤더 `send()` 하는 중
- `payload_send` : 페이로드 `send()` 하는 중

예외 상황
- `if_error` : `recv()` / `send()` 가 `SOCKET_ERROR` 반환
- `if_client_exit` : 클라이언트가 연결 종료 (`recv()` == 0)

이렇게 6가지의 `bool` 형 변수로 상태를 정의했어.

이건 오직 **송 / 수신 과정에서의 오류를 확실하게 판별하기 위한** 상태 관리 구조체라,
송 / 수신 과정 이외의 과정과 오류는 딱히 넣지 않았어.

왜 송 / 수신 과정에서의 오류만 판별하려하냐면, 

일단 송 / 수신을 담당하는 `send()` / `recv()`가 
이 프로젝트에서 **이중 반복문을 `break`해야하는 유일한 과정**이기 때문이야.

다른 과정인
- `socket()`
- `bind()`
- `listen()`
- `accept()`
- `closesocket()`

이것들은 오류가 발생해도 굳이 이중 반복문을 `break`할 필요가 없어.

- `socket()` / `bind()` / `listen()`까지는 아예 이 함수들이 서버의 기반을 깔아주기 때문에 
이 함수들이 실행되지 않으면 아예 서버 자체가 가동이 안돼. 
그래서 프로그램을 껐다가 켜는게 이득이고,

- `accept()`는 오류가 발생하면 그저 반복문 continue해서 다시 `accept()`를 호출하는 식으로 진행하면 되고,

- `closesocket()`도 `accept()`와 비슷하게 오류가 발생하면 다시 호출해주면 될 것 같고.

오직 
- `recv()`
- `send()`

이것들만 
- 모든 데이터가 송 / 수신될 때 까지 계속 `send()` / `recv()` 하는 반복문 
(send / recv 한 번 만으로 모든 데이터가 전송된다는 보장이 없으니)
- 클라이언트가 연결 종료했을 때 / 오류가 발생했을 때까지 계속 송 / 수신을 반복해주는 반복문
(한 번만 send / recv 하고 끝낼게 아니니까)

이렇게 **두 개의 반복문을 연속해서 break**해야해.

그리고 이 함수들에서 오류가 나도 계속 서버는 돌아갈 수 있고,
서버를 다시 껐다 키는게 오히려 비효율인 상황이 발생해.
다시 `socket() -> bind() -> listen() -> accept()` 까지 해야하니까..

그리고 나중에 **멀티 클라이언트 구조**로 설계한 경우에서는,
한 클라이언트의 송 / 수신 과정에서 오류가 발생했다고 
서버를 종료해서 모든 클라이언트의 연결을 끊어버리는건
너무 수지타산이 안맞으니까..

그리고 `recv()` 함수는 추가로
- **클라이언트가 연결을 종료**했을 때
- **`SOCKET_ERROR`가 반환되어 에러가 발생했음을 탐지**했을 때 

이 두 상태도 확실히 구분을 해야 디버깅이 상당히 쉬워지겠지.

그래서 어차피 
- `send()`에서 오류가 났든 
- `recv()`에서 오류가 났든 
- `recv()`가 0을 반환했든

셋 다 반복문을 두 개 break해야하고,
애초에 `send()` / `recv()` 는 오류났다고 해서 서버를 껐다 키는게 더 손해고,
`recv()`는 거기에다가 반환값이 `SOCKET_ERROR`인지, `0`인지까지 구분해야해서
딱 `recv()` / `send()` 에서의 상태만 기록해놓는거야.