# fs-cache

- batching write/read syscalls using iouring, in the kernel overwrite write/read syscall with batching.
    maintain the block to file mapping in the map.
- batching send/recv syscalls using iouring, in the kernel overwrite send/recv syscall with batching.
    maintain the shared resources in the map.
