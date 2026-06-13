## 0. 서론

솔직히 지금 이거 쓸 머리가 없어서,
그냥 설계 문서 복붙 + 설계를 구현한 코드 복붙으로 처리해야할 것 같아.

그래도 

- 어떤 문제가 있었고
- 그 문제를 해결하기 위해 어떤 설계를 택했고
- 그 설계의 구현은 어떻게 했는지

는 다 나와있으니,
문제는 없겠지.

지금 뇌가 타버릴 것 같아서
그냥 복붙만 하고 자러감..

[깃허브 링크](https://github.com/SiRyus1111/MultiThreaded-Echo-Chat-Server)

---

# 대개편

## 기존 구조의 문제

기존에는 `RecvPacket()`이 해당 수신 작업의 결과를 `NetState` 형태로 반환했다.

이 구조는 단순히 패킷을 받고, 수신 과정에서 문제가 있었는지를 확인하는 단계에서는 충분했다.
하지만 패킷 타입이 늘어나고 패킷 핸들러를 도입하려고 하자 문제가 생겼다.

패킷 핸들러는 수신한 `PacketType`을 기준으로 알맞은 처리를 해야 한다.
그런데 `RecvPacket()`이 `NetState`만 반환하면,
정상적으로 수신한 패킷의 `PacketType`과 페이로드를 호출자에게 명확히 전달하기 어렵다.

특히 HEADER_ERROR 처리에서 문제가 드러났다.

헤더 자체가 깨졌거나 정의되지 않은 `PacketType`을 받은 경우는 `RecvPacket()` 내부에서 protocol error로 처리하는 것이 맞다.
하지만 상대가 `HEADER_ERROR` 타입의 패킷을 정상적으로 보낸 경우는 수신 실패가 아니라,
정상적으로 수신한 에러 알림 패킷이다.

따라서 `NetState`는 송수신 과정의 성공/실패(transport error, protocol error, peer closed)만 나타내고,
`PacketType`은 정상 수신된 패킷의 의미를 나타내도록 책임을 분리해야 한다.

---

### 짧게 다시 이 문제가 발생했는지 써보자

1. 패킷 핸들러를 만들려고 함.
2. 패킷 핸들러는 패킷 타입을 보고 그 패킷 타입에 맞는 처리를 함
3. 그런데 HEADER_ERROR인 패킷은 이미 RecvPacket()에서 NetState에 기록하는 식으로 처리되고 있었음
4. 충돌남. 패킷 핸들러 vs RecvPacket() 중 어디에 처리해야하는지 명확히 책임 분리를 할 필요를 느꼈음

> 그래서 RecvPacket() 에서에 예외 기록은 
수신 자체에 문제가 있는 경우만 기록하기로 정함 그리고 예외 처리 함수에서 처리함

> 정상적으로 패킷이 수신됐을 때는 전부 패킷 핸들러에서 처리함

---

### 어딜 수정해야할까?

(서버랑 클라이언트 둘 다 해야함!)

1. 일단 서버와 클라이언트에 `RecvPacket()`이 반환할 `RecvResult` 구조체를 따로 만든다.
    - `NetState` `state`
    - `PacketType` `type`
    - `uint32_t` `length`
    - `std::string` `payload`
2. `RecvPacket()`에서 `HEADER_ERROR` 검사하는 부분을 뺀다. 즉, `RecvPacket()` 내부의 오류로 처리하지 않는다.
    - `HEADER_ERROR` 타입의 패킷도 정상적인 타입의 패킷 수신으로 보고, 따로 `HandleRecvPacket()`을 할 때 해당 타입에 맞는 동작을 실행함으로써 처리한다.
3. `NetState`에서도 `peer_protocol_error`를 뺀다. 
    - (이건 제일 나중에 하자. 어차피 남아있어도 문제는 없어. 안 쓰면 그만이니까. 마지막에 하자.)
    - (이게 서버와 클라이언트에서 공통으로 사용하는거라 수정했을 때 빨간줄 무서움..)
    - `peer_protocol_error`는 `HEADER_ERROR` 타입의 패킷을 수신했을 때만 `true`인 플래그이기 때문에 뺴는게 맞다.
4. `RecvResult` 구조체를 처리하는 `HandleRecvPacket()` 함수를 `ClientSession` / `ClientApp`에 만든다.
    - 패킷 핸들러의 밑바닥, 기반이라고 생각하고 만들어야하겠다 이건.
5. `HandleTransportException()` 함수도 개편이 필요한 부분을 개편해야한다. (특히 `peer_protocol_error` 처리 부분)
    - 이참에 기존에 이름이 `TransportExceptionHandling()` 이었던 `ClientSession`의 함수 이름도 `HandleTranspoerException()`으로 바꾸자.
    - `HandleTransportException()` 함수는 오직 전송 과정에서 발생한 문제(transport error / protocol error / peer closed)만 처리한다.
    - `HandleRecvPacket()` 함수는 정상적으로 수신된(유효한 패킷 타입이라면 패킷 타입에 관계없이) 패킷에 대한 처리를 한다.
6. `Run()` 함수 내부 `RecvPacket()`과 연관된 코드 싹 다 개편
    - 이건 안하면 병신
7. `HandleRecvPacket()` 함수, 즉 패킷 핸들러(패킷 타입에 따라 알맞는 처리를 해주는 함수)를 만들자.
    - `HEADER_ERROR` 처리를 개편하는 김에 이것도 한번에 해야겠다.
    - 자세한 목적 및 설계는 여기 참조 (링크가 있었으나 같은 문서라서.. 밑의 패킷 핸들러 참조)

---

## 개편된 구조

### 관계 정의

```text
RecvPacket() 내부의 NetState 기록
  → 통신 과정에서의 에러만 기록
  → 유효하지 않은 패킷 타입 / 유효하지 않은 length 필드의 값 등 패킷 수신 과정에서 발생한 에러만 기록

RecvResult
  → 수신의 결과
  → NetState = 수신 과정에서 문제가 발생하지는 않았는가
  → PacketType = 수신한 패킷의 타입이 어떤가
  → 이 두 가지 멤버를 확실히 분리해서 봐야한다

HandleRecvPacket()
  → 정상적으로 패킷이 수신되었을 때(HEADER_ERROR 타입의 패킷과는 상관 없이)의 후처리

HandleTransportException()
  → 정상적으로 패킷이 수신되지 않았을 때의 후처리

PacketType::HEADER_ERROR
  → 정상적으로 수신된 패킷
  → 정상적인 패킷(매우 중요)
  → 하지만 해당 세션 종료라는 후처리를 해야하는 패킷
```

---

### 상태 전이도(?)라고하나 이걸?

아 맞다 처리 흐름도라고 할 수 있겠네 이거.

(머메이드는 잘 몰라서 이걸로 대충 때움.)

```text
RecvPacket()이 정상적으로 수행되었는가? (RecvResult.state로 확인)
  → 그렇다면 HandleRecvPacket(RecvResult)
  → 아니라면 HandleTransportException(RecvResult.state)

HandleRecvPacket()에서는 각 패킷 타입에 맞는 처리를 한다.
  → 그렇기 때문에 RecvResult를 통째로 받는다.

HandleTransportException()에서는 각 수신 예외 상황에 대한 처리를 한다.
  → 그렇기 때문에 RecvResult 구조체의 state 멤버만 받아도 문제 없다(수신 예외 상황이 오직 state에만 있으므로)
```

---

## 결국 이 개편은 무엇을 의미할까.

> 기존의 클라이언트 세션 종료 vs 종료하지 않음의 기준으로 패킷 / 예외 처리를 했던 것과 달리,
> 현재의 개편된 패킷 처리 구조는
> 수신 중 예외 상황이 발생한 패킷 vs 수신 중 예외 상황이 발생하지 않고 정상적으로 수신된 패킷을 기준으로
> 각 패킷의 처리를 한다.

> 그렇기 때문에 peer closed 상황은 수신 중 예외 상황이 발생한 패킷이니까 NetState에 기록 후 HandleTransportException()에서 처리하고,
> HEADER_ERROR 헤더의 패킷를 수신할 때는 수신 중 예외 상황이 발생하지 않았으니 HandleRecvPacket()에서 PacketType을 보고 처리하는 것이다.

--- 

# 패킷 핸들러

```
패킷 핸들러 도입
  → 전제 조건: RecvPacket()이 수신한 PacketType을 반환해야 함
  → 그래야 수신한 PacketType을 보고 패킷 핸들러가 알맞은 처리를 할 수 있음
  → 서버랑 클라이언트 둘 다 해야함

닉네임 필드 추가 (ClientSession)
  → 패킷 핸들러가 NICKNAME_CHANGE를 처리할 때 저장할 곳 필요
  → 이건 이미 했음
  → 근데 일단 패킷 핸들러 구조부터 잡아놓고 NICKNAME_CHANGE 처리를 그 후에 만들어야 할 듯.

브로드캐스트 payload 형식 설계
  → 누가 보낸 메시지인지 포함
  → 이건 일단은 헤더의 필드로 따로 분리하기 어려울 것 같으니 페이로드 맨 앞에 따로 [NickName] 느낌으로 붙여서 보내기
  → 이거 때문에 클라이언트 메시지 출력 정책을 설계해야함..

send_mutex
  → 브로드캐스트 구현 직전에 추가
  → 아직 브로드캐스트 구현은 멀었으니까 지금은 추가하지 않음
```

## 패킷 핸들러가 뭘까?

난 이렇게 생각함.

> 패킷 타입에 맞춰서
알맞은 처리를
빠르게 할 수 있게,
그리고 편하게 다른 패킷 타입이 추가될 때 확장할 수 있게
PacketType-method 느낌으로 
`수신한 패킷의 타입-해당 패킷을 수신했을 때의 동작`으로 묶어놓은거.

막 unordered_map마냥, 해당 unordered_map을 순회하다가
if (받은 패킷의 PacketType == 해당 unordered_map의 PacketType)이라면 해당 unordered_map의 value인 함수를 실행하는거..

```cpp
// 이런 느낌?
std::unordered_map<PacketType, std::function(...)>

// 이렇게 PacketType을 key로, 해당 패킷 타입일 때 해야할 동작을 value으로 놓고
// 받은 PacketType이 해당 unordered_map의 특정 패킷 타입과 일치하면 value인 function(동작)을 실행하는거..
```

근데 이거 switch-case문으로도 할 수 있지 않을까ㅋㅋ 모르겠다ㅋㅋ

switch-case 문으로 구현해봐야겠다.
따로 `HandleRecvPacket()` 함수 만들어서..

---

## 패킷 핸들러는 어디에 들어가야할까?

일단 `RecvPacket()` 후는 확정인 듯.
일단 패킷을 받은 후에 해당 패킷의 패킷 타입에 맞는 처리를 하게 되니..

그 떄 unordered_map을 순회하면서 해당 패킷 타입을 계속 key들과 대조해보는거지.

그리고 key와 일치하면 해당 key의 value로 등록되어있는 함수를 실행하고..

switch-case도 비슷하게 각각의 PacketType을 case로 놓고 
수신한 패킷의 패킷 타입을 보게 하자.

---

## 실제 구현에서 문제가 될만한 곳이 있을까?

일단 내가 `std::function`같은걸 써본 적이 없음.
그래서 간단하게라도 배워야할 듯..

여기서 내가 의도한 설계가 적용이 안될 수 있음.

근데 switch-case 쓰기로 했으니 이 문제는 적어도 지금은 없겠네ㅋㅋ

---

## 패킷 타입 별 처리는 어떻게?

- `CHAT_MESSAGE` : 현재는 해당 메시지를 다시 돌려보내기.
- `CHANGE_NICKNAME` : 현재는 `ClientSession`의 닉네임을 바꾸고 `CHANGE_NICKNAME_SUCCESS` / `CHANGE_NICKNAME_FAIL` 패킷을 다시 클라이언트에게 보내기.
  - 추가 : 그리고 클라이언트에서는 `CHANGE_NICKNAME_SUCCESS` 패킷을 받아야 `ClientApp::nickname`을 바꿔야 할 듯. 그 전에는 `ClientApp::nickname`을 건들면 안됨. 확실히 서버에서 변경 성공 패킷이 날아왔을 때만..
- `HEADER_ERROR` : 후처리 함수 호출..

아 시발 모르겠다 진짜!!!!!!!!
이게 지금 `HEADER_ERROR` 처리가 `RecvPacket()` 내부에서 검사 후 `peer_protocol_error = true`를 하는데..
시발 이거 어쩌냐.. 책임 분리를 어떻게 해야하지???
나를 죽이시오.. 시발ㅜㅜ

(이거를 해결한게 대개편임)

---

### HEADER_ERROR와 NetState 개편.. 시발 ㅈㄴ 하기 싫음

기존 구조의 문제:

- RecvPacket()이 NetState만 반환하기 때문에, 정상 수신된 PacketType을 바탕으로 패킷 핸들러를 호출하기 어렵다.
- 또한 HEADER_ERROR 타입의 정상 패킷과, 수신 과정에서 발생한 protocol error가 NetState 안에서 섞이는 문제가 있다.

개선 방향:

- RecvPacket()의 반환값을 RecvResult 구조체로 변경한다.
- RecvResult는 NetState, PacketType, payload, length를 함께 가진다.
- NetState는 송수신 과정의 성공/실패만 기록하고,
- PacketType은 정상 수신된 패킷의 의미를 나타내도록 책임을 분리한다.

이거 완료함. 잘 돌아갈 듯?
(위의 대개편 설계를 적용한 후 이 메시지 적음)

---

### 패킷 핸들러 실제 구현 - switch-case문

switch-case문을 사용해서 받은 RecvResult를 기반으로 처리를 하는 `HandleRecvPacket()` 함수를 만들자.
`ClientSession` / `ClientApp`에.

- `ClientSession::HandleRecvPacket(RecvResult res)`
  - `res.type == PacketType::CHAT_MESSAGE`
    - 다시 클라이언트에 해당 메시지 SendPacket()
  - `res.type == PacketType::HEADER_ERROR`
    - 해당 클라이언트 세션 종료 (외부에서는 closing 검사로 해당 세션 종료 상태를 판별)
  - `res.type == PacketType::NICKNAME_CHANGE`
    - 아직 미설계. 그냥 임시로 남겨놓음.

---

### 패킷 핸들러 구현 중 문제 - 종료해야할 때는 어떻게 함?

이것도 문젠데..ㅋㅋ

기존에는 그냥 종료해야할 때 while문 안에 그냥 있으니 break하면 끝이었는데,
이제 따로 `HandleRecvPacket()`으로 뻈으니 `Run()` 함수에 따로 종료 여부를 알려줘야함..

어차피 종료하고 나서 같은 `SessionId`인 클라이언트가 접속될 것은 아니니까,
그냥 `HandleRecvPacket()`에서 종료해야하면 논리적 종료 상태 플래그인 `closing`을 `true`로 바꾸고, 
`Run()` 함수에서 매번 `closing == true`인지 확인하고 `closing == true`라면 `break`하는 식으로 종료해야할 경우를 처리하게 하자..

어차피 `closing`의 의미가 `해당 클라이언트 세션의 논리적 종료 상태`니까,
받은 패킷 타입에 의해(예 : `HEADER_ERROR`) 종료해야하는 경우도 논리적 종료 상태라고 할 수 있는 거니까..

그냥 while문에서 `closing == true` 확인해서 종료하는 용도로도 `closing` 플래그를 써도 될 듯.

---

#### `ClientApp`에 `closing` 추가

물론 `ClientApp`에는 `closing`이 없는데,
그냥 이번 기회에 추가하죠?

의미는 `ClientSession::closing`과 같이 `해당 클라이언트 세션의 논리적 종료 상태`로 잡아놓고.

물론 `closing`을 `true`로 변경하는 `MarkClosing()`도 추가해야하고..

#### `closing` 검사 시점

받은 패킷 처리가 끝난 후에 종료해야하는지 안해야하는지 확인해야하니까
`HandleRecvPacket()` 함수 호출 후에 `closing == true`인지 검사함.

- `closing == true` : while문 break
- `closing != true` : 그대로 계속 실행