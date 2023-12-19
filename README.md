## Multithreading components

### Single thread bounded queue

Ring buffer implementation.

### Single thread unbounded queue of polymorphic objects

Store polymorphic objects of dynamic types that are derived from some base type, using pointers to base type.

`push` member function should be explicitly specialized with dynamic type to construct.
It uses single allocation for queue control structures and constructed object.

### MPMC bounded queues

Multiple producers, multiple consumers ring buffers implementations.

### Reusable resources pool

Multiple consumers bounded pool of reusable resources.
Its main purpose to reuse resourses that are expensive to create each time its needed.
e.g. locked (pined) memory buffers

### Thread pools

Fixed signature thread pool, allocation free.

Arbitrary signature thread pool, single allocation per task.

## License
This software is licensed under the [BSL 1.0](LICENSE.txt).