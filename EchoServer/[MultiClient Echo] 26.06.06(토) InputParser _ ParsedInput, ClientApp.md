## 0. 서론

이번엔 
- 클라이언트에서 사용자에게 받은 입력을 어떻게 처리해야하는지 가공해주는 InputParser 객체
- 그 입력을 어떻게 처리해야하는지를 담는 ParsedInput 구조체
- 그리고 클라이언트의 정보 / 동작을 묶어주는 ClientApp 객체

이렇게 세 가지의 객체를 설계 및 구현해봤어.

### 0-1. 이걸 하게된 계기

계기라고 한다면,
- 닉네임 시스템 설계

지.

기존의 클라이언트 구조로는
- 일반 채팅 패킷
- 닉네임 설정 패킷

을 따로 나눠서 보낼 수가 없는 구조였거든.

더 정확히는,

> 사용자로부터 입력받은 메시지로 다른 동작으로 처리할 수가 없는 구조

> 사용자로부터 입력받은 메시지를 바로 일반 채팅 메시지로 인식해서
일반 채팅 패킷으로 보내버리는 구조

였어.

그래서
- 클라이언트에서 사용자에게 받은 입력을 어떻게 처리해야하는지 가공해주는 InputParser 객체
- 그 입력을 어떻게 처리해야하는지를 담는 ParsedInput 구조체

가 필요했어.

그리고 ClientApp은,
> 그냥 겸사겸사 클라이언트 리팩토링하는김에

했어. 
이유는 솔직히 그냥 이게 끝이야.

좀 더 자세히 설명해보자면, 기존의 코드를 보고, 
서버의 ClientSession처럼 해당 클라이언트의 정보 / 동작을 객체 하나에 묶어서
정보 캡슐화 / 동작 추상화라는 목적을 달성할 수 있겠더라고.

그리고 그 동작들을 별개의 인터페이스로 묶어서 편의성(추상화로 인한)도 높이고,
해당 클라이언트의 정보들을 해당 ClientApp 객체 외부에서 접근할 수 없게 해서(캡슐화),
오직 해당 ClientApp 객체의 함수로만 접근할 수 있게 해서
안전성도 높이고.

### 0-2. 객체들의 역할

이것도 딱 정해놨어.

```text
ClientApp
  → 해당 클라이언트의 정보 / 동작을 모아놓은 객체

InputParser
  → 입력을 파싱해서 입력의 의미를 나타내는 ParsedInput을 반환하는 객체

ParsedInput
  → 파싱된 입력, 즉 해당 입력으로 처리해야하는 작업을 나타내는 객체
```

요렇게.

각 객체는 본인의 역할을 벗어나는 처리를 하지 않아.

- InputParser는 그저 입력의 의미를 ParsedInput의 형태로 해석해주는 역할만 하고,
- ParsedInput은 입력의 의미를 나타내는 역할만 하고,
- ClientApp이 송 / 수신 작업을 처리해주고.

이런 식으로.

## 1. InputParser / ParsedInput

일단 최종 코드 먼저.
(사실상 InputParser에는 Parse() 함수밖에 없으니까 함수 내부 구현까지 넣음)
```cpp
// 입력받은 문자열의 첫 n글자(식별자)에 따라 클라이언트에서 입력받은 메시지의 동작을 분리

// 입력받은 메시지를 파싱한 결과
struct ParsedInput {
    PacketType type; // 오직 보내야할 패킷 타입만 나타냄
    uint32_t length; // 보내야할 페이로드의 길이를 나타냄
    std::string payload; // 실제로 보낼 메시지를 나타냄(/nick같은 메시지 식별자 절삭한)

    bool quit = false; // 종료 메시지냐 (이것 먼저 검사)
    bool valid = true; // 이 파싱된 결과가 유효하냐 (이것 먼저 검사)
};

class InputParser {
private:

public:
    static ParsedInput Parse(std::string& input) {
        
        ParsedInput parsed_input;

        if (input.empty()) {
            parsed_input.valid = false;
            return parsed_input;
        }
        
        if (input[0] != '/') { // 일반 메시지인 경우   

            parsed_input.type = PacketType::CHAT_MESSAGE;

            parsed_input.payload = input;

            parsed_input.length = input.size();

            return parsed_input;
        }

        // 식별자를 봐야하는 메시지인 경우

        if (input == "/quit") { // 종료
            parsed_input.quit = true;
            return parsed_input;
        }

        if (input.starts_with("/nick ")) { // 닉네임 변경

            std::string nickname = input.substr(6); // "/nick "다음 문자열을 nickname으로 복사
            if (nickname.empty()) {
                parsed_input.valid = false;
                return parsed_input;
            }

            parsed_input.type = PacketType::NICKNAME_CHANGE;
            parsed_input.payload = nickname;
            parsed_input.length = nickname.size();

            return parsed_input;
        }

        // 추후에 다른 식별자 추가 가능

        // 여기까지 오려면 input이 !(일반 메시지 || /nick으로 시작하는 메시지 || /quit으로 시작하는 메시지)여야 함.
        // 즉, 유효하지 않은 메시지.
        parsed_input.valid = false;
        return parsed_input;
    }
};
```

### 1-1. InputParser

(Parse() 함수)

일단 처음에는 각 동작의 식별자부터 생각해봤어.
최종적으로는,
- (첫글자가 /이 아님) : 일반 채팅 메시지 패킷 전송
- /nick nickname : 닉네임 설정 패킷 전송 (닉네임 안 적으면 유효하지 않은 동작)
- /quit : 종료 동작
- /(정의되지 않는 명령어) : 유효하지 않은(Invalid) 동작
- (아무것도 없음, 바로 엔터) : 유효하지 않은(Invalid) 동작

이렇게, /로 일반 메시지와 명령어를 구분하는 식별자 규칙을 잡았어.

1. 그리고 코드를 보면 먼저 바로 엔터를 찍은 동작부터 확인해서 
일단 뭐라도 입력했는지(아무것도 안 쓰면 유효하지 않은 동작)를 확인 후,

2. 첫글자가 /이 아니라면 바로 일반 채팅 메시지로 ParsedInput 구조체를 세팅해서 반환,

3. 그리고 첫글자 후 각 식별자마다 맞는 구조로
ParsedInput 구조체를 세팅해서 반환.

4. 그리고 /(정의되지 않은 명령어) 형태는 어차피 앞에 있던 
각 동작의 식별자들 다 거르고 나서
그 때까지 if문에 안 걸리면 /(정의되지 않은 명령어)라는 걸로 잡아서
맨 끝에서 valid = false로 세팅해서 ParsedInput 구조체를 반환.. 

이런 로직으로 짰어.

그리고 굳이 해당 함수가 객체를 생성해야만 호출할 수 있다면
입력을 파싱하는데 상당히 불편해질 것 같아서,
따로 static 키워드를 붙여서 객체를 생성하지 않아도
외부에서 호출할 수 있도록 했어.

### 1-2. ParsedInput

이건 InputParser::Parse() 함수의 실행 결과,
즉 입력을 어떻게 처리해야하는지를 담는 구조체인데,

일단 처리해야할
- 패킷 타입(PacketType type)
- 페이로드 길이(uint32_t length)
- 페이로드(std::string payload)

이 세 가지는 필수적으로 담고,

- 패킷을 송신하지 않을 경우,
- 즉, 입력이 종료 동작을 나타내거나(/quit)
- 입력이 유효하지 않은 경우

를 따로 구분해줬어.

그래서

- 종료해야하는 경우를 나타내는 플래그(bool quit, 기본값 false)
- 유효한 입력인지를 나타내는 플래그(bool valid, 기본값 true)

도 따로 ParsedInput 구조체에 담았지.

왜 그랬냐면,
일단 유효한 입력인지를 알아야 해당 입력에 대해 알맞는 처리를 할 수 있고,
패킷을 송신하지 않는 경우에는 패킷 타입으로 어떤 처리를 해야할지 
나타내기 어려웠기 때문이야.

그래서 ParsedInput 구조체를 검사할 때는
```text
valid -> quit -> PacketType
```

순으로, 
> 일단 해당 입력이 유효한지 
-> 패킷을 안 보내도 되는 종료 입력인지 
-> 패킷을 보내야한다면 어떤 패킷을 보내야 할지

순으로 검사해.
무조건 이렇게 해야함..

### 1-3. 개발 노트

여기 참조. 여기에 사고 과정들이 담겨있음.

[개발 노트 링크](https://github.com/SiRyus1111/MultiThreaded-Echo-Chat-Server/blob/main/IdeaScatch/26.06.04%20Inputparser%20%EA%B5%AC%EC%A1%B0%EC%B2%B4%20%EC%84%A4%EA%B3%84.md)

## 2. ClientApp

이것도 최종 코드 먼저.
(함수들은 별개로)
```cpp
class ClientApp {
private:
    ConnectSocket sock_;
    NetState state_; // 클라이언트 전체 상태 (생애주기 추적용)
    // send_state / recv_state는 각 송수신 호출의 결과 스냅샷
    Nickname nick_;
public:
    
    ClientApp(ConnectSocket s) : sock_(std::move(s)) {

    }
    
    void HandleTransportException(NetState state);

    ClientApp operator=(ClientApp&) = delete;
    ClientApp(ClientApp&) = delete;

    void Run();

    NetState SendPacket(const char* msg, uint32_t len, PacketType type);

    NetState RecvPacket(char* buf);
};
```

### 2-1. ClientApp의 필요성과 레퍼런스

기존의 클라이언트 코드는
입력 / 송신 / 수신.. 등등이 다 main() 함수 내부에 섞여있었어.

그래서 서버의 `ClientSession`처럼 하나의 객체로 클라이언트의 정보와 동작을 모아놓을 수 있다면 좋을 것 같다고 생각했어.

그리고 `Inputparser` / `ParsedInput`이 추가된 김에,
겸사겸사 따로 `ClientApp`을 추가하게 됐어.

솔직히 이건 문제로 인한 필요성 때문에 만들었다기보다는
**이 객체를 만듬으로써 얻을 수 있는 이득**에 집중했어.

- 클라이언트의 정보들을 객체 내부에 private 접근제어자로 안전하게 외부에서 건드리지 못하도록 캡슐화
  - 외부에서 해당 정보들을 건드리지 못하고 내부에서만 건드릴 수 있게 해서
  안전성을 높일 수 있음
- 클라이언트가 연결이 된 후에 하는 동작들을 따로 한 객체에 모아 추상화
  - 한 객체의 함수들만으로 클라이언트에 대한 조작을 추상화해서 편의성을 높일 수 있음
  
그리고 레퍼런스는, 서버의 `ClientSession` 객체로 잡았어.

서버의 `ClientSession` 객체를 보고,

- 클라이언트와 서버의 차이점 때문에 어떤 정보 / 동작은 필요없을지
- 클라이언트와 서버의 차이점으로 인한 추가적인 정보 / 함수는 있을지

를 생각하면서 `ClientApp` 객체를 만들었어.

### 2-2. ClientSession과 ClientApp의 차이점

이건 방금 말한 것처럼
> 클라이언트와 서버의 차이점

에서 생기는 문제들이야.

제일 큰 차이점은,
서버는 입력받는게 수동적(네트워크로 온 패킷을 받고 처리함)이지만,
클라이언트는 능동적(유저가 직접 한 입력을 받아서 처리함)이라는거야.

그래서, 결국 클라이언트에는

- 입력 파서(InputParser)
- 종료 상태(/quit, invalid input)

이런 것들을 따로 신경을 써줘야했어.

그리고 서버는 클라이언트 여러개를 관리해줘야하지만,
클라이언트는 딱 하나의 클라이언트라는 점도 있어.

그래서 클라이언트는

- 매니저에서 해당 클라이언트 세션의 논리적 종료 상태를 나타내는 closing 플래그
- 매니저에서 해당 클라이언트 세션을 식별하는데 사용하는 세션 ID
- 매니저에게 shared_ptr을 받아와서 weak_ptr를 만들기

이런 것들이 필요가 없었어.

저런것들은 결국 서버가 클라이언트 세션들의 상태를 관리하고,
클라이언트 세션들을 식별하고,
클라이언트 세션이 매니저에 간섭하기 위한 정보들이니까.

그리고 당연히 그 점에서 

- `MarkClosing()`(closing 플래그를 true로 바꿔주는 함수)
- `RemoveThisClient()`(세션 ID로 ClientManager에 해당 클라이언트를 관리 목록에서 제거해달라고 요청하는 함수)
- `AddToManager()`(ClientManager에서 shared_ptr 받아와서 weak_ptr 초기화하는 함수)

이런 함수들은 필요없게 돼.

### 2-3. ClientApp이 소유하는 것들과 동작

#### 소유하는 것들

- `ConnectSocket` `sock_` : 이건 필수. 해당 클라이언트가 서버와 통신할 때 사용하는 소켓이니까 필수.
  - 근데 이건 절대 복사로 못 가져오기 때문에 생성자에서 move semantic으로 가져오도록..
- `NetState` `state_` : 이것도 반필수. 해당 클라이언트의 현재 상태를 기록하는데 좋을 듯.
- `NickName` `nick_` :   
  - 현재 클라이언트가 사용 중인 닉네임을 로컬에서도 기억하기 위한 멤버.
  - 닉네임 변경 패킷을 보낸 뒤, 클라이언트 측 출력이나 상태 표시에서 사용할 수 있는 느낌.
  - 하지만 결국 클라이언트의 닉네임 권한 자체는 서버에 있음.
  - 그저 로컬 저장용.

`InputParser::Parse()`는 `static`이라 외부에서도 접근할 수 있으므로 소유하지 않음.

#### 동작

- `NetState SendPacket(const char* msg, uint32_t len, PacketType type)`, 
  - 추후 다른 패킷에 대한 Send 함수 제작 시 `SendChatMessagePacket()` 
  - 패킷 송신 함수. 필수. 추후 다른 패킷을 송신할 때도 이 함수가 원형.
  - 상태 기록 및 반환 정책은 `ClientSession`의 `SendPacket()` / `RecvPacket()` 함수와 같음. 
    - 해당 `ClientApp`의 전체적인 상태를 기록하는 `state_`
    - 해당 `SendPacket()` / `RecvPacket()` 송 / 수신의 결과를 기록하는 `send_state` / `recv_state`
- `NetState RecvPacket(char* buf)`, 추후 서다른 패킷에 대한 Recv 함수 제작 시 `RecvChatMessagePacket()` : 패킷 수신. 필수. 추후 다른 패킷을 수신할 때도 이 함수가 원형.
  - 여기도 상태 기록 및 반환 정책은 `ClientSession`의 `SendPacket()` / `RecvPacket()` 함수와 같음
- `void Run()` 
  - 에코 서버 단계에서는 
  `입력 받기` -> `파싱 및 예외 입력 처리` -> `send() + 예외처리` -> `recv() + 예외처리` -> `출력`
  으로 이어지는 루프만 구현하면 됨. 
  - 전체적인 클라이언트 동작을 나타내는 함수. 현재로써는.
- `void HandleTransportException(NetState state)`
  - 서버의 `ClientSession:TransportExceptionHandling()`에 대응하는 함수.
  - 송 / 수신 후 발생한 예외 상황에 대한 후처리

### 2-4. Run() 함수의 전체 흐름

이게 ClientApp에서 제일 중요하지. 사실상 클라이언트의 흐름이니.

```text
1. 사용자에게서 getLine() 으로 입력받기
2. 입력받은 메시지를 InputParser::Parse() 함수로 ParsedInput의 형태로 의미를 추출하기.
3. ParsedInput을 보고 예외 처리하기
4. SendPacket() 및 예외 처리
5. RecvPacket() 및 예외 처리
6. RecvPacket() 으로 받은 메시지 출력
```