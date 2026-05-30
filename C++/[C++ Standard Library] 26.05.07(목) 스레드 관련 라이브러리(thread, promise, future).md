## 0. 서론

이제 내 프로젝트 전 공부의 제일 헬인 구역에 진입했네.

1. 객체 표현과 객체의 생명 주기(완료)
2. C++의 RAII(완료)
3. 스마트 포인터(완료)
4. 스레드 관련 라이브러리 기본(이번 글)
5. 동기화 관련 라이브러리 기본

스레드 관련 라이브러리..
뭔가 딱봐도 머리아파지는 것 같아.
~~(사실은 동기화 관련 라이브러리가 훨씬 어려움)~~

이번 글에서는 Josuttis 책의 18.2챕터(`thread`/`promise`)와 18.1챕터의 일부분(`future`)을 보고 공부했어.

- `std::thread`
- `std::promise`
  - `packaged_task<>`
- `std::future`

이 정도를 공부할 것 같아.

이번 글의 목차는,

1. Class `std::thread`
2. Class `std::promise` & Class `std::future`
3. Class `packaged_task<>`

이렇게 3가지 챕터로 C++ 표준 라이브러리의 스레드 관련 라이브러리들에 대해 설명해볼게.

그리고 이 책은 C++11기준임.
C++11이후의 C++버전들은 다루지 않아서 딱히 그 부분들은 없어.

이 글의 내용을 정말 짧게 요약하자면,
- `std::thread` = 실행의 단위, 우리가 아는 소프트웨어적 스레드와 관련되어있음.
  - `std::thread` 객체가 실제 소프트웨어적 스레드는 아님. 그저 핸들. 
  - 함수를 인자로 넣음.
  - 그 함수의 필요한 인자들도 인자로 넣음.
  - 참조 전달할 때 `std::ref()` 써야함.
- `std::promise` & `std::future` = 스레드 간의 상호작용을 도와주는 데이터의 통로(channel)
  - `std::promise` = 보내는 쪽
  - `std::future` = 받는 쪽
  - `std::promise`와 `std::future`는 `1 : 1`로 매칭됨
- `std::packaged_task` = 함수의 반환값을 자동으로 `std::promise`에 넣어주는 Wrapper
  - 함수의 리턴값을 알아서 `std::promise`에 넣어줘서 `std::future`가 받을 수 있게 해줌

이 정도라고 할 수 있겠네..

## 1. Class `std::thread`

복습하는 차원에서
일단 스레드란,
쉽게 말해서
> 프로세스의 `힙 영역` / `코드 영역` / `데이터 영역` / `파일` 등을 공유하고
`스택 영역` / `레지스터 값`들은 분리되어서
각각 실행되는 실행 흐름

이라고 할 수 있어.

그러니까 결국 스택 / 레지스터만 분리되어있고
프로세스의 힙 / 코드 / 데이터 / 파일 등을 공유하니

한 프로세스 내에서 여러 개의 실행 흐름을 가진다고 생각하면 돼.
하지만 지역 변수들은 공유하지 않는거지.
물론 참조나 포인터로 넘기면 공유할 수는 있어.
하지만 이걸 어떻게 넘기느냐도 중요한데, 이건 좀 이따 보자.

그리고 `new`로 동적 할당된 객체나 전역 변수(`static`같은거, 아니면 그냥 전역에 선언된)는
공유를 해.

그런 것들은 스레드끼리 공유하는 힙 영역에 선언되어있거나(동적 할당된 객체)
아예 데이터 영역에 선언되어있거든(`static`으로 선언된 객체)(프로그램의 수명동안 유지됨).
전역에 선언된 객체도 물론 스코프가 해당 소스 파일에서는 전역이니 뭐..

어쨌든 이런 스레드를 다룰 수 있는 C++의 인터페이스가 `std::thread`야.

---

## `std::thread`의 사용법과 동작

### 1-1. `std::thread`의 초기화(스레드 생성)

`std::thread`는 생성자에 실행할 함수를 넘겨주면,
그 함수를 새로운 스레드에서 실행시켜줘.

```cpp
#include <thread>

void dosomething() {
    // ...
}

int main() {
	std::thread t(dosomething);
}
```
물론 함수에 넣을 인자가 필요한 경우에는
```cpp
#include <thread>

void dosomething(int num, std::shared_ptr<T> sp) {
    // ...
}

int main() {
	int n = 10;
    std::shared_ptr<T> t_sp = std::make_shared<T>();

	std::thread t(dosomething, n, t_sp);
}
```
이렇게 생성자의 두 번째 이상 인자들에 넣어서
해당 함수의 인자들을 넣는 것과 동일한 동작을 하게 할 수 있어.

중요한 점은, 
`std::thread` 객체를 생성하는 순간 아예 OS가 관리하는 새로운 실행 흐름이 시작된다는 점이야.
단순히 객체만 생성하는게 아니라, 
**OS차원에서 해당 함수를 실행하는 스레드를 생성**하는거지.

그러니까 `std::thread`의 생성자는
**OS에 새로운 실행 흐름, 즉 스레드를 생성**하라고 하는거야.

그리고 이렇게 함수의 인자들을 `std::thread` 객체의 생성자에 넣을 때는 주의점이 있는데,
아무것도 명시를 하지 않는다면 
이 값들은 기본적으로는 copy(복사)로 `std::thread`객체에 넘어가. 
그리고 따로 move semantics를 명시를 해주면 move(이동)로 해당 객체에 넘어가.
그 후 실제 스레드, 즉 함수로 넘어가게 되지.

더 정확히는, `std::thread` 객체의 생성자에 넘긴 인자들은
바로 함수로 들어간다는게 아니라
`std::thread` 객체가 일단 자기쪽에 decay-copy 형태로 저장해두고,
생성된 새 스레드에서 그렇게 저장된 값들을 **`r-value`로 캐스팅**(이거 중요함)한 후 
해당 함수에 매개변수로 넘겨주는 식으로 함수를 호출한다는거야.

결국 바로 함수에 들어가는게 아니라,
`std::thread` 객체에 한번 저장된 후 새 스레드에서 사용되는거야.
여기서 아무것도 명시를 하지 않는다면 인자들은 copy로 해당 함수에 넘어가게 된다는거지.


물론 
- 일반적인 값 복사
- `shared_ptr` 
- raw pointer(`int*`)

이런 것들은 큰 문제는 없어.

그리고 
- `unique_ptr`

이거는 복사할 수 없으니 `std::move()`로 넘겨야겠지.
이건 확실히 move semantics(이동 동작규칙)를 명시를 해줬으니까 문제는 없어.

하지만 여기서 중요한게,
- reference(`int&`)

이건 어떻게 해야할까?
reference 변수는 `std::thread`의 생성자에 넘길 때 자동으로 복사되겠지.
근데 그러면 reference의 의미가 없잖아?

받는 함수에서 reference로 받는다고 해서,
`std::thread`의 생성자는 어차피 copy를 해서 넘겨줄텐데..
그러면 넘겨진 reference 변수는 copy된 값을 받게 되고,
원래의 변수의 변화에 영향을 받거나 줄 수는 없겠지..

실제로는 보통 컴파일 에러가 나.
```cpp
#include <thread>

void dosomething(int& num) {
    // ...
}

int main() {
	int n = 10;

	std::thread t(dosomething, n); // 보통 컴파일 에러
}
```
코드를 보면, 
애초에 `std::thread`에 복사된 `n`이 새 스레드에서 `dosomething` 함수에 전달될 때,
`r-value`, 즉 컴파일러가 "곧 사라질 임시 값"으로 인식하는 형태로 캐스팅해서 전달해.
그런데 해당 함수의 `int&`가 요구하는 "수정가능한 l-value reference"와는 잘 맞지 않아.
곧 사라질 값인데 해당 값을 참조하고 수정하는 것은 이상하잖아?
컴파일러는 그래서 프로그래머의 실수로 판단해.
그래서 애초에 보통은 컴파일 에러가 발생하게 되지.

그래서 따로 "이 값은 수정가능한 l-value reference로 넘기겠습니다~"라고 명시하는
`std::ref()`라는 함수로 `std::thread`의 생성자에 넘겨야 해.

그러니까 `std::ref()`의 의미는,
> 이 값은 복사해서 넘길 값이 아니라, 원본 객체를 참조해야 하는 값이다.

라고 생각하면 돼.

그리고 `std::ref()`의 내부 동작은,
그냥 reference를 감싸고 있는(wrapping) 객체라고 생각하면 돼.

물론 raw pointer는 단순히 `std::thread` 객체에 생성자에 넘기는 것 자체는 문제가 없는게,
raw pointer는 단순히 주소값을 저장하기 때문에
주소값을 복사해서 넘겨도 역참조로 해당 주소에 접근할 수 있기 때문이야.

물론 해당 raw pointer가 가리키는 객체의 수명에 대해서는 조심해야겠지.
(그래서 이런거는 해당 객체의 수명에 따라, 소유권에 따라 동작하는 스마트 포인터를 주로 써.)
(아니면 해당 객체의 수명이 논리적으로 해당 스레드가 실행되는 동안에는 유지된다는 전제가 있거나..)

---

### 1-2. `std::thread`의 수명 관리 - `join()` / `detach()`

근데 여기서 필요한게,
> 해당 `std::thread` 객체가 스코프를 벗어나서 소멸됐을 때,
어떻게 처리를 할 것인가.

야.

앞에서 말했듯이,
`std::thread` 객체는 실제로 OS레벨의 스레드가 아니고,
단순히 해당 스레드를 나타내는 객체, 
즉 실제 스레드인 **OS레벨의 스레드의 핸들(Handle)** 느낌이야.

```
std::thread != 실제 스레드
```
라는거지.

그래서 `std::thread` 객체가 스코프를 벗어나서 소멸되었다고 해도,
실제 OS레벨의 스레드는 멈추지 않고 실행되고 있을 수 있다는거야.

실제 동작하는 스레드는 OS레벨의 스레드고.

예시를 한번 들어보자면,
```cpp
#include <thread>

void dosomething() {
    // ...
}

int main() {
    {
    	std::thread t(dosomething);
    }
    // 스코프를 벗어나서 std::thread 객체는 소멸
    // 하지만 실제로 실행되고 있는 OS의 스레드는 어떻게 해야할까?
}
```
이거야.

여기서 선택지는 이 정도가 있어.

1. 해당 스레드와 `std::thread` 객체가 결합(`join`)되어있는 상태로 
**해당 스레드가 종료될 때까지 기다리기**.
2. 그냥 해당 스레드를 `std::thread` 객체와 **분리(`detach`)**시켜서 
`std::thread` 객체가 사라져도 **스레드가 계속 실행**될 수 있게 하기.
3. 그냥 **프로그램 종료**(`terminate`)시키기. 
    - (근데 이건 선택지라기보다 아무것도 선택 안 했을 때의 C++의 최종 안전장치 느낌..)

그리고 C++에서는 프로그래머가 이런 것들을 명시적으로 결정해줘야해.

그리고 그걸 담당하는 함수들이
- `join()`
- `detach()`
- (`terminate()`)

야.
`terminate()`는 왜 괄호쳐놨냐면,
이건 사실상 예외 상황일 때 자동으로 호출되는거라,
명시적으로 호출할 일은 거의 없기 때문이야.

사실상 선택지는 두 개지.
1. `join()`으로 해당 스레드가 종료될 때까지 `join()`을 호출한 스레드의 진행을 멈춘다.
2. `detach()`로 해당 스레드와 `std::thread` 객체를 분리시켜서 해당 스레드를 독립적으로 실행시킨다.

둘 중 아무것도 하지 않은 상태로 
아직 스레드와 결합되어있는(`joinable`) `std::thread` 객체가 소멸되면
C++은 자동으로 `std::terminate()`를 호출해줘. 
이건 정말로 최종 안전장치 느낌..

실제로 C++은 
`join()` / `detach()` 함수를 이용해서 `std::thread`의 수명을 관리하는 것을 권장해.

막간으로 `joinable`이란,
앞에서처럼 `std::thread` 객체가 아직 스레드를 나타내고 있는,
스레드와 결합되어있는 상태를 의미해.

즉, `std::thread` 객체에 대해서 아직 `join()`도 `detach()`도 호출하지 않아서,
해당 객체가 나타내고 있는 스레드의 종료를 
기다릴지(`join()`), 분리할지(`detach()`)를 아직 결정하지 않은 상태라고 볼 수 있어.

그리고 `joinable()` 함수로 해당 `std::thread` 객체가 `joinable`한지 확인할 수 있지.

정리하자면,
> `std::thread` 객체의 수명이 끝나기 전에,
해당 객체가 나타내고 있던 스레드를 어떻게 처리할지 반드시 관계를 정리해야한다.

> `std::thread` 객체의 수명 관리란,
`join()`과 `detach()`를 사용해서
C++ 객체와 실제 OS레벨의 스레드 사이의 관계를 정리하는 것이다.

라고 할 수 있겠네.

이제 `join()`과 `detach()`, 그리고 +@로 `terminate()` 함수까지 알아보자.

### 1-2-1. `join()` - 해당 스레드가 종료될 때까지 기다리기

`join()` 함수는 앞에서 말한대로,
> 생성된 스레드가 종료될 때까지 `join()`을 호출한 스레드의 진행을 멈추는 함수

라고 할 수 있어.

join() 함수는
1. 이 스레드의 종료를 내가 확인해야 한다.
2. 이 스레드가 종료된 뒤에 다음 작업을 해야한다.
3. 이 스레드가 접근한 데이터의 수명을 안전하게 맞춰야 한다. (이게 어찌보면 진짜 중요)

주로 이런 목적으로 사용돼.

그리고,
- 스레드가 수행한 작업의 결과가 필요함 = 1, 2
  - (스레드가 수행한 작업의 결과는 스레드가 종료되어야, 작업이 끝나야 나옴)
  - (물론 결과는 단순 `join()`이 아닌 `promise`/`future`같은 다른 수단을 사용해야 전달이 됨)
  - (그래도 `join()` 함수는 해당 스레드의 작업이 끝났다는 것을 보장해줌.)
- 공유 데이터 정리 전에 해당 스레드가 종료되었다는 보장이 필요함 = 1, 3
  - (해당 공유 데이터에 해당 스레드가 의존성이 있는 경우)
  - 어떤 객체를 스레드가 사용하고 있을 때
  해당 객체가 소멸되기 전에 해당 스레드가 더 이상 그 객체를 사용하지 않는다는 보장이 필요함
  - 이 때 `join()`을 통해 스레드의 종료를 보장할 수 있음
- 프로그램 종료 전에 스레드들을 안전하게 정리해야함 = 1, 2

주로 이런 경우에 사용돼.

요약하자면,
> `join()`은 해당 스레드가 수행하는 작업의 끝을 확실히 알아야 할 때 사용된다.

라고 할 수 있어.

코드로 예시를 들어보자면,
```cpp
#include <thread>

void dosomething() {
    // ...
}

int main() {
    {
        std::thread t(dosomething);
        t.join(); // 해당 스레드(t)가 종료될 때까지 다음 코드가 실행되지 않음
    }
    // 다음 코드들
}
```

이렇게, 생성한 스레드가 종료될 때까지 다음 코드가 실행되지 않는거야.
결국, 해당 스레드를 끝까지 책임진다는거지.

이건 어떻게 사용할 수 있냐면, 
> TaskA는 별도 스레드에서 실행하고,
main 스레드는 그동안 독립적인 작업을 하다가,
TaskA가 끝난 뒤에만 TaskB를 실행해야 하는 상황

이런 상황일 때,
```cpp
#include <thread>

void TaskA() {
	// 오래 걸리는 작업
}


void TaskB() {
	// TaskA가 끝난 뒤에 해야하는 작업
}

void IndependentTask() {
    //  TaskA와 상관없이 동시에 할 수 있는 작업
}

int main() {
	std::thread threadA(TaskA);
      
    IndependentTask(); // 딱히 TaskA를 신경쓸 필요 없는 작업
    
    // ...
    // main 스레드는 TaskA가 실행되는 동안 다른 작업을 할 수 있음
    
    threadA.join(); // TaskA의 종료를 보장
    
    TaskB(); // TaskA가 끝난 뒤에 TaskB 시작
    
    return 0;
}
```
이렇게 실행될 순서를 보장해줄 수 있는거야.

이걸 좀 더 확장하면,
어떤 스레드 B가 다른 스레드 A의 완료를 기다려야 하는 상황에서도 `join()`을 사용할 수 있어.

즉, 스레드 B 안에서 스레드 A를 나타내는 `std::thread` 객체에 대해 `join()`을 호출하면,
스레드 B는 스레드 A가 종료될 때까지 그 자리에서 진행을 멈추고,
스레드 A가 끝난 뒤에야 다음 작업을 이어서 실행할 수 있는 식으로..

마치 Task Graph?
어떤 작업의 시작이 다른 작업의 완료에 의존하는 구조를 만들 수 있으니까..

#### 정리

정리하자면, `join()` 함수는 단순히 
> 생성된 스레드가 종료될 때까지 `join()`을 호출한 스레드의 진행을 멈추는 함수

일 뿐만 아니라,
> 생성된 스레드의 종료를 기다리고 확인하는 함수

라고도 할 수 있어.

근본적으로는
> 생성된 스레드와 C++의 `std::thread` 객체 사이의 관계를
**"C++은 생성된 스레드가 종료될 때까지 기다린다."**

라고 관계를 정리해주는 함수야.

특히 `join()`을 호출한 뒤의 코드에는 해당 스레드가 종료됐다는 보장을 해줌으로써,
`join()` 이후의 작업들은 해당 스레드가, 해당 작업이 끝났다는 전제를 가지고
다음 코드를 실행할 수 있게 해줘.

### 1-2-2. `detach()` - 해당 스레드를 분리시키기

`detach()` 함수도 앞에서 말한대로
> 생성된 스레드와 `std::thread` 객체를 분리시켜서 해당 스레드를 독립적으로 실행시키는 함수

라고 할 수 있어.

`join()` 함수와 다르게,
`std::thread` 객체가 생성된 스레드를 책임지지 않아.

아예 `std::thread` 객체와 OS레벨의 실제 스레드가 분리(detach)되어서,
별개로 실행되는거야.

즉, 실제 스레드는 `std::thread` 객체의 존재 여부와 상관없이 알아서 돌아간다는거지.

`detach()` 함수는
1. 이 스레드는 다른 스레드들과 독립적으로 실행되어야 한다.
2. 이 스레드는 백그라운드에서 실행되어야 한다.
3. 이 스레드가 종료될 때 스레드의 관련된 자원(스택 값, TCB 등)의 회수를 다른 스레드에 묶지 말아야 한다.

이런 목적으로 사용돼.

그래서,
- 메인 흐름이 스레드의 완료를 기다릴 필요가 없음(예시 = 로깅, 모니터링) - 1, 2
  - 굳이 해당 스레드가 완료된 것을 `join()`같은 함수로 확인하지 않아도
  전체 스레드들의 흐름에는 문제가 없는 경우
  - (`detach()` 함수는 생성한 스레드를 아예 메인 흐름에서 분리해서 독립적으로 실행될 수 있게 해줌)
  - (물론 단순히 해당 스레드의 실행 결과가 필요할 때는 `promise` / `future`같은 수단을 사용해 전달함)
- 시간이 많이 걸리는 작업을 백그라운드로 실행하고, 메인 흐름은 즉시 다음 입력을 받아야 함 - 1, 2
  - (예시 ) 파일 다운로드 같은 오래 걸리는 작업을 백그라운드로 실행하기)
  - (`detach()` 함수는 생성한 스레드를 메인 흐름에서 분리해서 독립적으로 실행될 수 있게 해줌)
  - (물론 결과(예시에서는 다운로드 완료)를 `promise` / `future`같은 수단으로 메인 흐름에 알려야하는 경우가 많음)
- 생성된 스레드의 관련 자원 정리를 다른 스레드에 묶지 않고 알아서 정리되는 구조가 필요함 - 3
  - (그저 스레드가 종료될 때(return) OS가 알아서 스레드 관련 자원을 싹 정리하기 때문에
  굳이 `join()`처럼 스레드 자원 뒷정리를 다른 스레드에서 할 필요가 없음.)

주로 이런 경우에 사용돼.

요약하자면,
> `detach()`는 어떤 목적이든 다른 스레드들과 해당 스레드를 분리해야할 때 사용된다.

근본적으로는 이렇다고 할 수 있어.

이건 어떻게 사용할 수 있냐면,
> 메인 스레드에서 의존성이 없는 않는 긴 작업(데이터 다운로드)을 실행하면서 
메인 스레드는 계속 다른 작업을 해야하는 상황

```cpp
#include <thread>

void IndependentDataDownload(){
	// 오랜 시간이 걸리는 데이터 다운로드 작업
    // 메인 스레드는 이 작업에 의존성이 없음 (메인 스레드의 흐름과 상관없음)
}

void AnotherTask() {
	// 메인 스레드가 계속 처리해야하는 작업
}

int main() {
	{
    	std::thread download_thread(IndependentDataDownload); // 정말 긴 시간이 걸리는 스레드
        
        download_thread.detach();
        // detach() 했기 때문에 스코프를 벗어나게 되더라도 download_thread 객체는 사라지지만
        // 분리된 실제 스레드는 정상적으로 실행됨
        // 하지만 download_thread 객체를 사용해서 실제 스레드를 관측할 수는 없음
    }
    // 실제 IndependentDataDownload가 실행되는 스레드는 문제 없이 실행 중
    
    
    while (true) {
    	AnotherTask();
    }
    
    return 0;
}
```

#### `detach()`의 주의점

하지만 `detach()`는 `join()`과 다르게 사용 시 꼭 신경써야하는 주의점이 있어.

`detach()`를 실행하게 된다면 
`std::thread` 객체, 즉 스레드 핸들과 실제 스레드 간의 연결이 끊어지기 때문에,
`std::thread` 객체로 해당 스레드의 종료 여부 등을 알 수 없어져.

즉, 해당 스레드와 연결되어있는 스레드 핸들(`std::thread` 객체)이 없기 때문에
`std::thread` 객체를 통해 해당 스레드를 관측하거나, 해당 스레드가 종료될 때까지 기다릴 방법이 없어지는거야.

`join()`은 확실하게 해당 스레드의 종료 여부를 확인할 수 있지만,
`detach()`는 그게 안된다는거지.

그래서 내가 현재 진행하고 있는 멀티클라 에코서버 프로젝트에 적용한 것 처럼
```cpp
class ClientSession {
private:
	// 다른 멤버는 생략
    std::atomic<bool> closing = false;
public:
	// 다른 메서드는 생략
    void Run() {
    	while(true) {
        	/* 
        	대충 송 수신 과정
        	*/
        }
        
        // 송 / 수신 과정에서 에러 발생시 실행하는 코드
        closing.store(true);
    }
};

void client_thread(std::shared_ptr<ClientSession> s) {
	s->Run();
    
    return;
}

int main() {
	// 앞 부분은 생략
    
    while (true) {
    	// accept() 하는 코드
        
        std::shared_ptr<ClientSession> client = std::make_shared<ClientSession>();
        
    	std::thread ClientThread(client_thread, client);
        
        ClientThread.detach();
    }
}
```

이렇게 따로 `closing`같은 해당 스레드의 상태를 알려주는 변수로
해당 스레드의 상태를 외부에(이 코드에서는 `ClientSession` 객체) 기록하는 식으로
해당 스레드의 상태를 기록하던지,

아니면 뒤에 나올 `promise`와 `future`를 이용해서
해당 스레드가 종료되거나 특정 상태가 되었을 때
`promise`로 메인 흐름(스레드)에 알려주는 식으로
해당 스레드를 직접 관리해야해.

물론 `promise` / `future`는 `join()` 처럼 해당 스레드 자체를 제어하는 수단은 아니야.

물론 이렇게 하면 딱히 분리하는게 아니라는 생각도 들 수 있지만,
결국 실제 스레드는 `std::thread` 객체(스레드 핸들)로부터 분리되어있어서,
결국 **해당 스레드의 제어권 자체는 포기하지만,
상태는 공유**하는 느낌이야.

그리고 가장 중요한 주의점이,
해당 스레드가 참조하는 객체의 수명을 보장해야한다는 점이야.

```cpp
#include <thread>

void thread(ClientSession* s) {
	// ClientSession 객체를 계속 역참조하는 코드
}

int main() {
	{
    	ClientSession client;
        // ClientSession 객체를 어찌저치 초기화
        
        std::thread ClientThread(thread, &client);
        
        ClientThread.detach();
    }
    // 스코프를 벗어나도 실제 thread는 제대로 실행되지만,
	// client는 이미 소멸되어있다.
    // detach된 스레드가 이후 &client를 역참조하면 댕글링 포인터 문제가 발생할 수 있다.
}
```

이렇게 해당 스레드가 참조하는 객체의 수명이 해당 스레드가 종료되기 전에 끝나게 된다면,
해당 객체가 이미 소멸된 객체를 참조하려 하는 댕글링 포인터 문제가 발생할 수 있어.

그래서,
```cpp
#include <thread>
#include <memory>

// ClientSesssion 객체를 shared_ptr로 넘겨받아서 
// 해당 객체에 대한 소유권을 thread 함수도 공유하게 되므로
// thread 함수가 종료되기 전에는 ClientSession 객체가 소멸되지 않는 것이 보장된다.
void thread(std::shared_ptr<ClientSession> s) { 
	// ClientSession 객체를 계속 참조하는 코드
}

int main() {
	{
    	std::shared_ptr<ClientSession> client = std::make_shared<ClientSession>();
        
        std::thread ClientThread(thread, client);
        
        ClientThread.detach();
    }
    // main 스레드 쪽의 client shared_ptr은 사라졌지만,
    // detach된 스레드 함수가 shared_ptr 복사본을 가지고 있으므로
    // thread()가 끝나기 전까지 ClientSession 객체는 살아있다.
    
    return 0;
}
```

이렇게 따로 해당 객체의 소유권을 `std::unique_ptr`을 사용해서 아예 넘겨주거나,
`std::shared_ptr`을 사용해서 공유하는 식으로
해당 객체가 스레드 종료 전에 소멸되는 것을 막아줘야해.

그러니까, `detach()`한 스레드가 참조하는 객체의 수명이 
`detach()`한 스레드가 살아있을 때 유지되도록
따로 처리를 해줘야 한다는거야.

그리고 `detach()` 함수는 프로그램 자체의 수명을 늘려주는 함수는 아니야.
즉, `detach()` 한 스레드가 아직 실행중이더라도 
프로세스가 종료되면 해당 스레드도 더 이상 정상적으로 계속 실행된다고는 볼 수 없어.

#### 정리

정리하자면, `detach()` 함수는 
> 생성된 스레드와 `std::thread` 객체를 분리시켜서 해당 스레드를 독립적으로 실행시키는 함수

라고 할 수 있어.

근본적으로는
> 생성된 스레드와 C++의 `std::thread` 객체 사이의 관계를
**"생성된 스레드를 `std::thread` 객체와 분리한다."**

라고 관계를 정리해주는 함수야.

### 1-2-3. `terminate()` - 그냥 프로그램을 종료시켜버리기

앞에서 말했듯이,
C++의 `std::thread` 객체와 실제 스레드 간의 관계를 정리해주지 않은 상태로,
그러니까 `joinable`한 상태로
`std::thread` 객체가 소멸되게 된다면
C++은, 그러니까 `std::thread` 객체의 소멸자는 
자동으로 `terminate()` 함수를 호출해서
아예 프로그램을 죽여버려.

```cpp
#include <thread>

void dosomething() {
	// ...
}

int main() {
	{
		std::thread t(dosomething);
    }
    // 스코프를 벗어나면서 std::thread 객체가 소멸됨.
    // std::thread 객체가 소멸되었는데도 
    // join() / detach() 를 호출하지 않음.
    
    // 실제 std::thread는 아직 실제 스레드와의 관계가 정리되지 않았으므로
    // std::terminate() 함수가 호출되며 프로그램이 종료됨.
}
```

이건, 솔직히 직접 호출하는 함수라기보다는
C++의 `std::thread` 객체와 실제 스레드 관의 관계가 정리되지 않았을 때의
최종 안전 장치 느낌의 함수야.

그리고 강조하는데 이건 컴파일러가 판단하는 문제가 아니고,
실행 중에 `std::thread` 객체의 소멸자가 호출될 때 발생하는 동작이야.
`std::thread` 객체가 소멸되기 전에 아무런 관계로도 정리되지 않았다면,
`std::thread` 객체의 소멸자는 
결국 어떻게 해당 `std::thread` 객체와 실제 스레드를 다뤄야 할지 알 수가 없어서,
사실상 에러 느낌의 `std::terminate()` 함수를 호출하는거야.

### 1-2-4. RAII를 적용해서 `std::terminate()`가 호출되지 않게 하기

전에 
> 자원의 획득과 해제를 객체 생명주기에 묶기

라는 개념의 **RAII**를 배웠었지?

여기에서도 RAII를 적용할 수 있어.

어쨌든 `std::thread` 객체가 소멸되기 전에 해당 객체와 실제 스레드 간의 관계를 정리해야
`std::terminate`가 호출되면서 프로그램이 종료되지 않잖아?

그런데 그걸 수동으로 하다보면 한 번쯤은 `join()`이나 `detach()`를 호출하는 것을 빼먹거나,
아예 잘못된 위치(이미 `std::thread` 객체가 소멸된 후 등)에서 호출하는 식으로 코드를 짜놓아서
결국 C++이 `std::terminate`를 호출하게 돼.

그래서, 
결국 RAII의 의도인
> 자원 해제 시점을 프로그래머가 신경쓸 필요 없게 만들기

를 그대로 적용할 수 있어.

`std::thread` 객체를 따로 다른 객체로 묶어서,
소멸자에서 `join()` / `detach()`를 호출하게 함으로써,
안전하게 해당 `std::thread` 객체를 `joinable`하지 않게 만들어줄 수 있어.

```cpp
// 구현 예시
class ThreadRAII {
private:
	std::thread t;
    
    Act action;
    
public:
	enum class Act {
		join,
		detach
	};
	ThreadRAII(std::thread thread, Act a = Act::join) : t(std::move(thread)), action(a) {}
    
    ~ThreadRAII() {
    	if (t.joinable()) {
        	if (action == Act::join) {
            	t.join();
            }
            else {
            	t.detach();
            }
        }
    }
    
    ThreadRAII(const ThreadRAII&) = delete;
    ThreadRAII& operator=(const ThreadRAII&) = delete;
    
    ThreadRAII(ThreadRAII&&) noexcept = default;
    ThreadRAII& operator=(ThreadRAII&&) noexcept = delete;
    
    std::thread& get() {
    	return t;
    }
};
```

## 1-3. `std::thread` 총정리 

`std::thread` 객체의 생성자에 스레드에서 실행할 함수와 매개 변수를 넣는 식으로
`std::thread` 객체를 생성할 수 있다.

`std::thread` 객체가 생성되면 OS레벨의 스레드도 생성되지만,
`std::thread` 객체가 OS레벨의 스레드를 의미하지는 않는다.

`std::thread` 객체는 실제 OS레벨의 스레드가 아닌, 그저 스레드 핸들에 가까운 객체이다.

`std::thread` 객체의 생성자에 넣어진 매개변수들은
`std::thread` 객체 내부에 복사 / 이동된 후, 
`std::thread` 객체와 연결된 
OS레벨의 스레드가 실행하는 함수에
`r-value`로 넘겨진다.

그래서 원본 객체를 참조로 넘기고 싶은 경우에는 `std::ref()` 함수를 사용해야 한다.

해당 `std::thread` 객체가 소멸되기 전에
`std::thread` 객체와 OS레벨의 스레드의 관계를 정리해줘야 한다.

1. `std::thread` 객체와 해당 스레드의 연결을 유지하며 해당 스레드의 종료를 기다린다.
    - `join()`
2. `std::thread` 객체와 해당 스레드를 분리하며 해당 스레드를 독립적으로 실행시킨다.
    - `detach()`
3. 앞의 두 가지의 경우로 `std::thread` 객체와 해당 스레드의 관계를 정리하지 않고
`std::thread` 객체가 소멸된다면
소멸자에서 프로그램 자체를 종료시킨다.
    - `std::terminate()`

쉽게 요약하면,

1. 끝까지 기다릴거면 `join()`
2. 분리해서 실행시킬거면 `detach()`
3. 아무것도 하지 않으면 `terminate()`

`detach()`를 사용할 때는
- 해당 객체를 `std::thread` 객체로 관찰할 수 없음
- 해당 스레드가 참조하는 지역 객체의 수명을 신경써야함
  - 물론 해당 지역 객체의 동기화는 별도임

라는 점들을 주의해야한다.

## 2. Class `std::promise` & Class `std::future`

지금까지 `std::thread` 클래스와
`join()` / `detach()` 함수를 배울 때
이런 의문이 들 수 있어.

> 결국 `join()`를 해서 해당 스레드의 종료를 기다리러라도 
그저 해당 스레드의 종료 여부를 알 수 있을 뿐
해당 스레드가 실행된 결과를 알 수 없지 않나?

> `detach()`를 했다면 `std::thread` 객체를 통해서 해당 스레드를 관측할 수 없는데,
그래도 해당 스레드의 실행 결과를 받아야 할 필요가 있지는 않을까?

이런 의문들.

여기서 중요한게,
- 스레드의 종료 여부

와
- 스레드의 실행 후 결과

는 다르다는 점이야.

결국 
- `join()`을 했다고 해도 해당 스레드의 종료 여부만 알 수 있을 뿐 실행결과를 알 수 없다.
- `detach()`를 했을 때는 `std::thread` 객체로 해당 객체를 관측할 수 없고,
해당 스레드의 실행 결과도 알 수 없다.

이 문제들에서 `std::promise`와 `std::future`가 시작해.

결국, `std::promise`와 `std::future`는 
> 한 스레드에서 `최종적으로 만들어진 결과값` / `발생한 예외` 등을
다른 스레드에서 나중에 받을 수 있게 해주는 통로(Channel)

역할을 하는 클래스들이야.

- `std::promise`로 보내고
- `std::future`로 받고..

그래서 이 둘은 따로 보기보다는,
아예 한 쌍의 클래스로 보는게 좋아.

## 2-1. 전체적인 `std::promise`와 `std::future`의 흐름

쉽게 딱 이 정도라고 생각하면 돼.

> `std::promise`와 `std::future`는 둘 사이에 존재하는 `shared state`를 통해 값을 주고받는다.

> 즉, `std::promise`가 직접 `std::future`에 값을 넘긴다기보다는
   `std::promise`가 `shared state`에 값을 저장하고
   `std::future`가 그 `shared state`에 저장된 값을 나중에 꺼내는 구조이다.
   
뭔가 익숙한 느낌이 들 수도 있는데,
나는
> `winsock2`의 `send()` / `recv()` 함수

가 먼저 떠오르더라..
- `send()` 함수는 상대 호스트에게 송신하는 함수가 아닌 
그저 송신 버퍼에 송신할 바이트열을 전달하는 함수
- `recv()` 함수는 상대 호스트에서 수신하는 함수가 아닌 
그저 수신 버퍼에서 수신할 바이트열을 읽어오는 함수

딱 이런 느낌이었어.
이렇게 중간에 다른 채널이 껴 있는 느낌?

일단 간단하게, 구어체 느낌으로 `std::promise`와 `std::future`의 흐름을 설명해보자면,

1. `promise` 객체를 만든다.
2. 해당 `promise` 객체와 연결된 `future` 객체를 `promise`로부터 얻는다.
    - `promise`가 입구, `future`가 출구인 단방향 회선 느낌
3. 결과값을 받아보려면 입구 역할을 하는 `promise`를 결과값을 보내야 하는 스레드에 넘겨준다.
4. 그 스레드는 실행을 한 후, 넘겨받은 `promise`에 실행의 결과값을 담는다.
5. 결과값을 받을 스레드는 `future`를 사용해서 그 결과값을 꺼낸다.

라고 할 수 있어.

이제 `std::promise`와 `std::future`의 흐름을 
좀 더 C++ 프로그래밍 관점에서 요약해보자면,

1. `std::promise<T>` 객체를 만든다.
2. `promise::get_future()` 함수로 해당 `std::promise<T>`와 연결된 `std::future<T>`를 얻는다.
3. 작업 스레드에 `std::promise<T>`를 넘긴다.
4. 작업 스레드는 `std::promise::set_value()` 함수로 
해당 `std::promise`와 `std::future`가 공유하는 `shared state`에 결과값을 저장한다.
5. 다른 스레드는 `std::future::get()` 함수로 그 결과값을 꺼낸다.

`결과값 저장 + 결과값 받기`를 흐름으로 보면 이런 느낌이지.

```
std::promise    std::future
	|              ^
    | set_value()  | get() 
   	v              | 
      shared state 
```

![](https://velog.velcdn.com/images/siryus0907/post/cb9adbc0-b8cd-4667-b2f3-25ae1b696678/image.jpeg)


## `std::promise`와 `std::future`의 사용법과 동작

### 2-2. 기본적인 사용법

C++에선 `<future>` 헤더를 인클루드해야
`std::promise`와 `std::future`를 사용할 수 있어.

---

그리고 `std::future`를 사용하기 위해선 우선 `std::promise` 객체를 만들어야해서,
일단 이렇게 `std::promise` 객체를 생성해줘.
`T`(타입)에는 해당 `std::promise` 객체가 전송할 메시지의 타입을 적어주면 돼.

```cpp
std::promise<T> p;
```

---

이제 `std::promise` 객체와 shared state를 공유하도록
해당 객체를 사용해서 `std::future` 객체를 생성해줘.
물론 타입도 맞춰주고.

```cpp
std::future<T> f(p.get_future());
```

---

그리고 스레드에 `std::promise` 객체를 넘겨줘.
해당 스레드의 결과를 `std::future`로 받아야하니까.

```cpp
std::thread t(dosomething, std::move(p));
```

`std::promise` 객체는 복사할 수 없어. 
출구(`future`)는 하나인데 입구(`promise`)가 여러 개라면 어느 입구에서 들어온걸 출구로 꺼내야할지 모르니까.

---

스레드 내부에서는 작업 결과가 나온 경우 `set_value()`로 
해당 `std::promise` 객체의 타입(`T`)과 같은 객체를 shared state에 넣어줘.

```cpp
// 스레드 내부
p.set_value("대충 해당 스레드 작업 결과"); // std::string 형의 promise라면 이렇게 std::string 형의 데이터를 넣는다.
```

---

만약 스레드 내부에서 예외가 발생했다면, 
`set_exception()` 함수로 해당 예외를 shared state에 넣어주고.

```cpp
// 스레드 내부에서 예외가 발생했다면
p.set_exception(std::current_exception());
```

---

그리고, 이렇게 `set_...()` 함수로 shared state에 데이터를 넣는건
딱 한 번만 가능하다는 점을 주의해야해.

---

그리고, `std::future` 객체로 결과값 / 예외를 받아야할 때,
`std::future::get()` 함수로 해당 결과값 / 예외를 shared state에서 꺼내올 수 있어.
하지만, 이건 딱 한 번만 가능해.

```cpp
T result = f.get();
```

`std::future`는 shared state의 결과를 단 한번만 꺼내올 수 있는
유일한 출구, 유일한 수신자 느낌이야.

그래서 `std::future` 객체는 복사할 수 없고,
결과값도 `get()`으로 딱 한번만 꺼내올 수 있는거야.

물론 공유 수신자 느낌의 `std::shared_future`도 있긴 한데,
이번 글의 범위가 아니니까 패스.

---

그리고 만약 `set_exception()`으로 예외가 shared state에 들어갔는데
`get()` 함수를 사용해서 shared state에서 값을 꺼낸다?
그러면 값을 반환하기 전에 해당 예외를 던져. 그러니까 `throw`해.
그래서 `set_exception()` 함수가 호출될 수 있는 작업의 결과를
`get()` 함수로 shared state에서 꺼낼 때는 예외처리가 필요하지.

---

그리고 만약 아직 shared state에 어떤 값도 들어있지 않아서 `get()`을 할 수 없다면,
`get()` 함수 호출 시점에서 대기하다가 shared state에 값이 들어오면 
`get()`이 실행되는 식으로 동작해.

---


지금까지 설명한걸 코드로 정리하면 이렇게 돼.
```cpp
#include <thread>
#include <future>
#include <iostream>
#include <string>
#include <exception>
#include <stdexcept>
#include <functional>
#include <utility>

void dosomething(std::promise<std::string> p){
	try {
    
    	// dosomething() 함수의 작업
        
        p.set_value("대충 dosomething() 함수 작업 결과");
    }
    catch (const std::exception& e) { // 예외가 발생한 경우
    	p.set_exception(std::current_exception()); // 현재 발생한 예외를 promise를 통해서 shared state에 집어넣기
    }
}

int main() {
	
	std::promise<std::string> p;
    std::future<std::string> f(p.get_future());
    
    std::thread t(dosomething, std::move(p));
    
    // 대충 다른 작업들
    
    try {
    
    	// t 스레드의 작업 결과를 보기 위해 std::future::get() 함수 사용
    	// set_value()가 정상적으로 호출됐다면 shared state에 있는 저장된 값을 그대로 읽어옴
    	// 하지만 set_value()든 set_exception()이든 호출되지 않았다면 대기
        
    	// set_exception()이 호출됐다면 해당 함수로 shared state에 저장된 예외를 
    	// std::future::get() 시점(std::string 반환 안함)에서 다시 던짐(throw)
        
    	std::string dosomething_result = f.get();
        
        std::cout << "작업 결과 : " << dosomething_result << '\n';
        
    }
    catch (const std::exception& e) {
    	std::cerr << "발생한 예외 : " << e.what() << '\n';
    }
    
    t.join();
}
```

1. 메인 스레드에서 `std::promise<std::string>` 객체를 생성한다.
2. 메인 스레드에서 해당 `std::promise` 객체와 shared state를 공유하도록 
`get_future()` 함수를 사용해서 `std::future<std::string>` 객체를 생성한다.
3. 작업 스레드(`t`, `dosomething()`)에 `std::promise<std::string>` 객체를 넘겨준다.
4. 작업 스레드(`t`, `dosomething()`)에서는 `set_value()` 함수를 이용해서
작업 결과를 해당 `std::promise<std::string>` 객체의 shared state로 넘겨준다.(단 한번만 가능)
  4-1. 작업 중 예외가 발생했다면 `set_value()` 함수를 이용하지 않고 
  `set_exception()` 함수를 이용해서
  발생한 예외를 해당 `std::promise<std::string>` 객체의 shared state로 넘겨준다.(단 한번만 가능)
5. 메인 스레드에서 다른 작업들을 실행하다가 해당 스레드의 결과를 보고 싶으면
`std::future<string>` 객체의 `get()` 함수를 실행해서 작업 결과를 꺼내온다.(단 한번만 가능)
  5-1. 만약 스레드에서 예외가 발생했다면 
  `get()` 함수 호출 시점에 발생한 예외가 던져지므로 예외를 처리해준다.
  5-2. 아직 shared state에 값이 넘겨지지 않았다면 `get()` 함수 호출 시점에서 대기한다.
  즉, 다음 코드가 실행되지 않는다.
  
이 정도.

### 2-3. shared state 좀 더 깊게 파보기

계속 shared state, shared state 하는데,
shared state가 도대체 정확히 뭘까?

일단 지금까지의 설명으로는
> `std::promise`의 `set_value()` / `set_exception()` 한 값이 
`std::future`의 `get()`으로 꺼내질 수 있도록 저장되는 중간 저장소

정도로 이해할 수 있을 것 같은데,
`std::promise`와 `std::future`를 알아보는데 정말 중요한 요소니까 좀 더 깊게 파보자.

### 2-3-1. shared state의 생성과 소멸

일단 shared state도 어쨌든 메모리에 할당되어있는 공간이야.
shared state는 힙 영역에 할당되어있어.

그리고 이런 의문을 가져본 적이 있을거야.

> shared state가 소멸되면 안되는 조건이 무엇일까?
`std::future`의 `get()` 함수가 호출되어 
shared state를 더이상 사용할 필요가 없게될 때 까지는 shared state가 남아있어야 할텐데..

> shared state는 언제까지 메모리에 남아있어야할까?
메모리 누수를 막으려면 필요없어진 shared state는 메모리에서 삭제되어야하는데..

답은 이 정도겠지.
> shared state는 
`std::promise` 객체가 생성될 때부터,
`std::promise`이 shared state에 `set_...()` 함수로 값을 넘겨주고,
`std::future`이 shared state에서 `get()` 함수로 값을 꺼내갈 때 까지 메모리에서 소멸되면 안된다.

> 그리고 앞에서 `std::future`이 `get()` 함수로 값을 꺼내간 후에 메모리에서 소멸되어야 한다.

결국 이 의문들은,
> 어떤 객체들이 shared state에 대한 소유권을 가져야 할까?
어떤 객체들이 shared state의 생명 주기에 관여해야할까?

로 확장될 수 있어.

즉, 전에 배웠던 스마트 포인터의 소유권 모델을 그대로 따라가.

shared state는 `shared_ptr`처럼 내부적으로 참조 카운트(Ref Count)를 관리해.
`promise`나 `future` 둘 중 하나가 소멸된다고 해서 바로 메모리에서 해제되지 않는다는거지.

쉽게 흐름을 설명하자면,

1. `std::promise` 객체가 생성되어 shared state가 힙 영역에 할당된다. 
(ref count = 1)
2. `std::promise` 객체의 `get_future()` 함수가 호출되어 `std::future` 객체가 생성된다.
(ref count = 2)
3. `std::promise` 객체가 `set_..()` 함수를 호출해서 shared state에 값을 넘겨준 후
스레드가 종료되어 해당 객체는 소멸한다.
(ref count = 1)
4. `std::future` 객체가 `get()` 함수를 호출해서 shared state에서 값을 꺼낸 후 
해당 `std::future` 객체는 shared state에 대한 소유권을 포기한다.
(ref count = 0)
5. 참조 카운트가 0이 되었기 때문에 shared state는 소멸한다.

이 흐름인거야.

### 2-3-2. shared state에 값이 저장되지 않은 채로 `std::promise` 객채가 소멸하면?

이 경우도 무조건 따져봐야겠지.

만약 shared state에 값이 저장되자 않은 채로 
값을 저장할 수 있는 `std::promise` 객체가 소멸되면 안되잖아?
그런걸 broken promise라고 하는데,
그러면 `std::future`는 대체 어떤 값을 받아야 할지 알 수가 없지.

그리고 이렇게 shared state에 값이 저장되어있는지 / 저장되어있지 않은지 여부는
상태 플래그(State Flags)로 관리돼.

상태 플래그란 결과값이 준비되었는지를 나타내는 bool 형의 변수야.
만약 `상태 플래그 = false`고 `참조 카운트 = 1`이라면
shared state에 값이 저장되지 않은 채로 `std::promise` 객체가 소멸한 상태겠지.

그래서 그럴 때는 그냥 예외를 발생시켜.
shared state가 소멸되어있진 않지만,
이미 `std::promise`가 소멸되어있고 shared state에 저장된 값이 없으니..

그래서 이럴 때 `get()` 함수를 호출한다면 `std::future_error`라는 예외를 던져.

### 2-3-3. `get()`과 `std::condition_variable`

조건 변수(`condition variable`)가 왜 나오나 싶을텐데,
> `std::promise`의 `set_...()` 함수로 값이 shared state에 저장되지 않았을 때
`std::future`의 `get()` 함수를 호출하면 값이 저장될 때까지 기다린다

이 동작 때문에 그래.

이 `값이 저장될 때까지 기다리다 값이 저장되면 값을 가져오는 동작`을
조건 변수로 구현하기 쉽기 때문이야.

1. `std::future`에서 `get()` 함수 호출 
    - shared state에 값이 저장되어있지 않아서 상태 플래그가 여전히 false(Not Ready)라면 
    내부 조건 변수를 통해 `get()`을 호출한 스레드를 대기 상태(blocking)로 만듬.
2. `std::promise`에서 `set_...()` 함수 호출 
    -  shared state에 값을 넘겨주고 상태 플래그는 true(Ready)로 바꿔주고
    내부 조건 변수의 `notify_one()`을 호출함.
3. `get()` 함수를 호출한 스레드를 깨움 
    - 대기 상태였던 스레드가 깨어나서 안전하게 shared state에 저장된 값을 꺼내가고,
    `get()` 함수가 반환됨
    
이렇게.

### 2-3-4. shared state 정리

shared state는 앞서 말한 세 가지 필드 + 값을 저장할 저장소(Payload)로 구성되어있어.

요약하면, 
- 실제 값이 저장되는 Payload(저장소)
- 소유권을 가진 `std::promise` / `std::future`의 갯수를 세어주는 Ref Count(참조 카운트)
- 결과값이 준비되었는지를 나타내는 State Flags(상태 플래그)
- `get()`의 완전한 구현을 위한 Synchronization(동기화 객체)

로 이루어져 있다는거야.

### 2-4. `std::promise`와 `std::future` 정리

`std::thread`의 `join()`으로는 해당 스레드의 종료 여부는 알 수 있지만
해당 스레드의 결과값 / 발생한 예외를 알 수는 없다.

`std::promise` / `std::future`는 스레드 사이에서 결과값 / 예외를 전달하는 도구이다.

`std::promise`의 `set_value()` 또는 `set_exception()` 함수를 호출하면
`shared state`에 결과값 / 예외가 저장되고,

`std::future`의 `get()` 함수를 호출하면
`shared state`에서 결과값이 꺼내진다.
예외라면 자동으로 던져진다.

`shared state`는 네 개의 필드가 포함되어있다.
- Payload(실제 데이터) 
  - 실제 값 / 예외가 저장되는 필드
- Ref Count(수명 관리 정보) 
  - 소유권을 가진 `std::promise` / `std::future`의 갯수가 저장된 필드
- State Flags(준비 상태) 
  - 결과값이 준비되었는지를 나타내는 필드
- Synchronization(동기화 도구) 
  - `get()`의 완전한 구현을 위한 필드

만약 `std::promise`가 소멸되었는데 결과값이 준비되어있지 않다면
`std::future::get()` 함수는 `std::future_error`를 던진다.

만약 `std::future::get()` 함수가 호출되었는데 
아직 `std::promise` 객체가 `set_value()` / `set_exception()` 함수를 호출하지 않았으면
`std::condition_variable`에 의해 `get()`을 호출한 스레드는 대기 상태가 된다.
그리고 `std::promise` 객체의 `set_value()` / `set_exception()` 함수가 호출됐다면
`get()`을 호출한 스레드가 깨어나서 `shared state`의 값을 꺼내간다.

## 3. Class `std::packaged_task<>`

앞의 `std::promise`를 보면,
불편한 점이 하나 있어.

바로,
> 일일히 결과 / 예외를 보내줘야하는 함수(작업 스레드 함수)에 매번 `std::promise`를 넘겨줘야 한다.

> 함수 내부에서 일일히 `try-catch`를 프로그래머가 직접 사용해서
결과값이나 예외를 `set_value()` / `set_exception()`으로 일일히 구분해서 
shared state에 넣어야 한다.

라는거지.

어차피 결과값은 함수(스레드)가 끝날 때 나오고,
예외는 만약 발생한다면 함수(스레드)가 종료되잖아?

그러니까 결국
> 함수(스레드의)의 종료

때에 자동으로 `std::promise` 객체를 통해서 shared state에 결과값이나 예외를 넣어주면 되잖아?

- 정상적으로 종료되었다면 `return`으로 반환된 값을 shared state에 넣으면 되고, 
- 예외는 내부에서 따로 감지할 수 있는 수단(`try`에서 예외를 감지하는 등)으로 
발생한 예외를 shared state에 넣어버리면 될텐데..

그걸 자동으로 해주는 클래스가 `std::packaged_task<>`야.

쉽게 말해 `std::packaged_task<>`란,
> 함수의 반환값(결과값)과 예외를 자동으로 `std::promise`를 통해서 shared state에 넘겨주는 도구

라고 생각하면 돼.

## `std::packaged_task<>`의 사용법과 동작

### 3-1. `std::packaged_task<>`의 초기화와 특징

`std::packaged_task<>`는 좀 독특하게, 템플릿 인자로 함수의 반환값과 매개변수를 받아.
```cpp
int positive_sum(int a, int b) {
	if (a < 0 || b < 0) throw std::invaild_argument("음수는 연산 불가.");
    
    return a + b;
}
int main() {
	// 함수의 반환값과 매개변수를 템플릿 인자로 넣음.
    // packaged_task로 함수를 감싸는 느낌.
	std::packaged_task<int(int, int)> sum_task(positive_sum);
    
	// 내부적으로 std::promise를 들고있기 때문에, std::promise처럼 future를 생성할 수 있다.
    std::future<int> f = sum_task.get_future();
}
```

여기서 중요한게,
- `packaged_task<>`도 내부에는 `std::promise` 객체가 있어서,
`get_future()`같은 함수를 통해서 같은 shared state를 가지는 `std::future`객체를 생성할 수 있다.
- `std::promise`처럼 복사는 불가능, 이동만 가능

이런 `packaged_task<>`의 특성도 고려해야한다는 점이야.

### 3-2. `std::packaged_task<>`를 스레드에 넘기는 법

이것도 간단해.
`std::thread` 클래스의 생성자는
첫 인자가 스레드에서 실행할 함수의 이름이잖아?

하지만 `std::packaged_task<>`는 복사 불가 객체고, 이동은 허용하니까
```cpp
std::thread t(std::move(sum_task), 1, 2);
t.join();
```
이렇게 `std::thread` 객체를 통해서 스레드에 넘길 수 있어.

그리고, `std::future`에서 `get()`으로 값을 꺼낼 때는
어차피 `std::packaged_task<>`에서 
자동으로 반환값 / 예외를 자동으로 shared state에 넣어주니까,
```cpp
try{
	int result = f.get();
    std::cout << "연산 결과 : " << result << '\n';
}
catch(const std::exception& e) {
	std::cout << "스레드 내부 예외 발생 : " << e.what() << '\n';
}
```
그저 이렇게 shared state를 공유하는 `std::future` 객체의 `get()` 함수로 꺼내주면 돼.
물론 예외는 `set_exception()` 함수로 넣어진 예외를 꺼낼 때 처럼
`get()` 호출 시점에 던져지지.

```cpp
int positive_sum(int a, int b) {
	if (a < 0 || b < 0) throw std::invalid_argument("음수는 연산 불가.");
    
    return a + b;
}
int main() {
	// 함수의 반환값과 매개변수를 템플릿 인자로 넣음.
    // packaged_task로 함수를 감싸는 느낌.
	std::packaged_task<int(int, int)> sum_task(positive_sum);
    
	// 내부적으로 std::promise를 들고있기 때문에, std::promise처럼 future를 생성할 수 있다.
    std::future<int> f = sum_task.get_future();
    
    // 스레드 생성
    // std::packaged_task<> 객체는 복사 불가, 이동 가능 객체이기 때문에
    // std::move()로 이동
    std::thread t(std::move(sum_task), 1, 2);
    t.join();
    
    // 예외가 던져질 수 있기 때문에 try-catch 사용
    try {
    	int result = f.get(); 
    	std::cout << "연산 결과 : " << result << '\n';
    }
    catch (const std::exception& e){
    	std::cout << "스레드 내부 예외 발생 : " << e.what() << '\n';
    }
}
```

이 코드의 `std::thread` 객체 생성부터 스레드 종료(반환)까지의 코드의 내부 흐름을 따라가보자면..

1. `std::packaged_task<>` 내부의 함수인 `positive_sum(1, 2)`를 실행한다.
2. 만약 정상적으로 값이 반환되면 내부의 `std::promise`를 사용해서 
`set_value(반환값)`을 자동으로 호출해준다.
3. 만약 함수 실행 중 예외가 발생하면 내부의 래퍼 가드에서 자동으로 `catch`해서 
`set_exception(std::current_exception())`을 수행한다.

이렇게 자동으로 `set_...()`를 실행해주는 객체라고 할 수 있어.

## 4. 종합 요약

솔직히 이거 이렇게 오래 쓸 글은 아닌 것 같긴 했는데(5/7 ~ 5/19)
중간중간에 프로젝트 글 쓰랴, 이산수학 글 쓰랴..
많이 펑크가 나서 이제야 다 썼네..

어쨌든 각 챕터의 핵심 문장들을 요약하며 마무리해볼게.

1. `std::thread` : 실제 실행 흐름인 OS레벨의 스레드를 제어하는 핸들 객체.
참조 전달 시에는 반드시 `std::ref()`로 전달해야한다.
해당 객체가 소멸하기 전에 `join()`이나 `detach()`로 관계를 정리해줘야 한다.
그렇지 않으면 `std::terminate()`가 호출되어 프로그램이 종료된다.


2. `std::promise` / `std::future` : 스레드의 결과값 / 예외를 다른 스레드에 보내주는
입구 / 출구 역할을 하는 객체.
중간에 `shared state`라는 중간 저장 공간을 거친다.
`std::promise`와 `std::future`가 `shared state`를 바라보고,
`shared state`에서 참조 카운트(`Ref Count`)를 활용해서 `shared state`의 수명을 관리한다.

3. `std::packaged_task<>` : `std::promise`의 단점인 프로그래머가 일일히 
`set_...()`을 호출해줘야 한다는 점을 보완한 함수(`task`) Wrapper 객체.
템플릿 인자로 반환값과 매개변수의 타입을 넣어서 초기화할 수 있으며,
스레드에 넣으면 자동으로 반환값 / 예외를 `shared state`에 넘겨준다.