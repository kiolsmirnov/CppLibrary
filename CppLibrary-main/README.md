<div align="center">

# 🧩 cpp-stl-from-scratch

**Собственные реализации ключевых компонентов стандартной библиотеки C++**

![C++](https://img.shields.io/badge/C%2B%2B-17%2F20-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)
![Header Only](https://img.shields.io/badge/header--only-yes-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-blue?style=flat-square)
![Status](https://img.shields.io/badge/status-stable-success?style=flat-square)

*Deque · SharedPtr / WeakPtr · IntrusiveList · StackAllocator*

</div>

---

## 📖 О проекте

Здесь собраны низкоуровневые реализации структур данных и утилит из C++ STL, написанные «с нуля» — с ручным управлением памятью, аллокаторами, устойчивостью к исключениям и корректной семантикой владения. Каждый компонент header-only и не тянет за собой ничего, кроме стандартной библиотеки.

Цель проекта — показать, как устроены классические абстракции STL изнутри: как `shared_ptr` считает ссылки без утечек, как `deque` умеет расти в обе стороны за O(1), и как аллокаторы могут полностью управлять тем, откуда берётся память.

---

## 📦 Состав библиотеки

| Файл | Компонент | Что внутри |
|---|---|---|
| [`Deque.h`](./Deque.h) | `Deque<T>` | Двусторонняя очередь на массиве блоков фиксированного размера |
| [`shared_ptr.h`](./shared_ptr.h) | `SharedPtr`, `WeakPtr`, `EnableSharedFromThis` | Умные указатели со счётчиком ссылок и общим control block |
| [`StackAllocator.h`](./StackAllocator.h) | `StackAllocator`, `StackStorage`, `List<T, Alloc>` | Аллокатор на стековом буфере и двусвязный список с аллокатором |

---

## 🚀 Быстрый старт

Все компоненты header-only — просто подключите нужный файл:

```cpp
#include "Deque.h"
#include "shared_ptr.h"
#include "StackAllocator.h"
```

Компиляция (стандарт C++17 или новее):

```bash
g++ -std=c++20 -Wall -Wextra -O2 main.cpp -o main
```

---

## 🔹 `Deque<T>`

Двусторонняя очередь произвольного доступа, построенная как массив указателей на фиксированные блоки (`kSubVectorSize = 16`). Такая схема даёт амортизированное **O(1)** для `push_back` / `push_front` / `pop_back` / `pop_front` и **O(1)** для доступа по индексу — без сдвига существующих элементов при расширении в любую сторону.

**Возможности:**
- Полный набор конструкторов: по умолчанию, по размеру, по размеру и значению, копирующий
- `operator[]`, `at()` с проверкой границ и выбросом `std::out_of_range`
- `push_back` / `push_front` / `pop_back` / `pop_front`
- Двунаправленные итераторы (`iterator`, `const_iterator`) с поддержкой `std::reverse_iterator`
- Строгая гарантия безопасности исключений при конструировании элементов
- Плейсмент-new / ручной вызов деструкторов — ни одной лишней инициализации

```cpp
Deque<int> d(5, 42);

d.push_front(1);
d.push_back(100);

for (int x : d) {
    std::cout << x << ' ';
}

std::cout << d.at(0) << '\n';   // безопасный доступ
d.pop_back();
```

---

## 🔹 `SharedPtr` / `WeakPtr` / `EnableSharedFromThis`

Собственный аналог `std::shared_ptr`, построенный вокруг единого полиморфного control block (`IBaseControlBlock`) с двумя реализациями:

- **`MadeSharedControlBlock`** — объект и блок управления выделяются одним куском памяти (аналог `make_shared`)
- **`DeleterControlBlock`** — поддержка пользовательских деleter'ов и аллокаторов для «сырых» указателей

**Возможности:**
- `makeShared<T>(...)` и `allocateShared<T>(alloc, ...)`
- Поддержка кастомных deleter'ов и аллокаторов
- Приведение указателей по иерархии наследования (`SharedPtr<Derived>` → `SharedPtr<Base>`)
- `WeakPtr` с `lock()`, `expired()`, корректным освобождением control block
- `EnableSharedFromThis` — безопасное получение `SharedPtr` на `this` изнутри объекта
- Корректное разделение `shared_count` / `weak_count` без двойного удаления

```cpp
auto sp = makeShared<std::string>("hello, shared world");
SharedPtr<std::string> sp2 = sp;             // shared_count == 2

WeakPtr<std::string> wp = sp;
if (auto locked = wp.lock()) {
    std::cout << *locked << '\n';
}

std::cout << "use_count: " << sp.use_count() << '\n';
```

```cpp
struct Node : EnableSharedFromThis<Node> {
    SharedPtr<Node> self() { return shared_from_this(); }
};

auto node = makeShared<Node>();
SharedPtr<Node> same = node->self();
```

---

## 🔹 `StackAllocator` и `List<T, Alloc>`

Аллокатор, выдающий память из заранее выделенного стекового буфера (`StackStorage<N>`), — без единого обращения к куче. Идеально подходит для тестирования аллокаторов на контейнерах, чувствительных к их поведению.

**`StackStorage<N>`** — буфер фиксированного размера `N` байт с выравниванием через `std::align`.

**`StackAllocator<T, N>`** — стандартный allocator-совместимый тип: `allocate`, `deallocate`, `rebind`, поддержка сравнения.

**`List<T, Alloc>`** — интрузивный двусвязный список на базовых узлах (`BaseNode` / `Node`), полностью совместимый с произвольным аллокатором:

- `push_back` / `push_front` / `pop_back` / `pop_front` / `insert` / `erase`
- Двунаправленные итераторы + `const_iterator`, `reverse_iterator`
- Поддержка `propagate_on_container_copy_assignment` и `select_on_container_copy_construction`
- Строгая гарантия исключений при вставке

```cpp
StackStorage<1024> storage;
StackAllocator<int, 1024> alloc(storage);

List<int, StackAllocator<int, 1024>> lst(alloc);

lst.push_back(1);
lst.push_back(2);
lst.push_front(0);

for (int x : lst) {
    std::cout << x << ' ';   // 0 1 2 — без единой аллокации в куче
}
```

---

## 🧠 Технические детали

- **Управление памятью:** ручное размещение объектов через placement `new` и явный вызов деструкторов, без избыточных конструкций по умолчанию
- **Безопасность исключений:** во всех конструкторах и модифицирующих операциях предусмотрен откат состояния при исключении в конструкторе элемента
- **Обобщённость:** каждый компонент параметризован аллокатором и корректно работает с `std::allocator_traits`
- **Совместимость с STL:** итераторы удовлетворяют требованиям `LegacyBidirectionalIterator`, что даёт бесплатную поддержку `<algorithm>`, range-based `for`, `std::reverse_iterator`

---

## 📄 Лицензия

Проект распространяется под лицензией MIT — используйте, изучайте и модифицируйте свободно.
