# Arena

`Arena` is a header-only thread-local arena allocator for C++23 that uses bump pointers to 
allocate dynamic memory from a pool of memory blocks. `Arena` also supports resizing logic by dynamically
 claiming more memory blocks when needed.

This reduces the need for expensive `malloc` overhead and heap fragmentation during run-time whilst simplifying 
lifetime management for common objects.

![resizable_alloc.png](assets/resizable_alloc_poor_cache_loc.png)

### Dev Log

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

In the revised implementation, we allow the `Arena` to hold raw pointers to `MemBlock`, 
 and we introduce a `cleanup` function for manual frees.

```c++
inline void cleanup() noexcept {
    clear();
    for (const MemBlock* blk : mem_blocks) {
        delete blk;
    }
}
```