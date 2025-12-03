# Arena

`Arena` is a header-only thread-local arena allocator for C++23 that uses bump pointers to 
allocate dynamic memory from a pool of memory blocks. `Arena` also supports resizing logic by dynamically
 claiming more memory blocks when needed.

This reduces the need for expensive `malloc` overhead and heap fragmentation during run-time whilst simplifying 
lifetime management for common objects.

![arenav2.png](assets/arenav2.png)

## Dev Log

### `ArenaV2`

#### Improved cache locality
In our original implementation, the ordering of destructor nodes are stored in a linked list across multiple 
memory blocks. The layout of ArenaV1:

![arenav1.png](assets/arenav1.png)

Unfortunately, when we clear `mem_blocks`, this requires us to manually traverse the linked list sequentially,
 where the linked list spans multiple `MemBlock`s.

To improve cache locality, we keep multiple `DestructorNode` in chunks of contiguous memory, `DestructorChunk`, and 
 maintain a linked list of `DestructorChunk` instead. This allows us to have flexibility in allocating more memory for
`DestructorNode`s whilst maintaining cache locality.

#### Reduced pointer indirection
We now directly track pointers to latest `MemBlock` and `DestructorNode`, allowing for faster access.

### `ArenaV1`

#### Removed virtual destructors
The destructor functions within each `DestructorBlock` were originally kept as type-erased virtual functions. 
```c++
[p] {static_cast<T*>(p)->~T();}
```
This means for each object added to `MemBlock`, we need to maintain a vtable entry, creating a layer of indirection. 
In the revised implementation, we declare a static template for the destructor function instead:

```c++
template<typename T>
static void destruct(void* p) noexcept {
    static_cast<T*>(p)->~T();
}
```
This generates a destruct function for each type during compile time instead, with a concrete implementation this also
 allows the compiler to directly inline the function call.


#### Reduced smart pointer overhead
The initial implementation utilises a vector of `std::unique_ptr` to manage `MemBlock` lifetimes. 

While this allowed for RAII, it also resulted in indirection penalty. Since the lifetime of 
our `Arena` is tied to each `MemBlock`, we never need to worry about sharing the pointer to other 
objects. Hence, paying this penalty is not really necessary. 

In the revised implementation, we allow the `Arena` to directly own `MemBlock`, and allow it to be
 automatically freed when `mem_blocks` is freed.