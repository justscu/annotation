annotation 
================
annotation for source code


# 1.nginx-1.4.4
## 1.1 ngx_pool_t，分为大内存和小内存。
  （１）当分配大块内存时，直接分配，最后有释放函数。
  （２）当分配小块内存时，是直接分配，没有释放函数的。
      这样做的原因，刚好符合http这种处理。当http来时，构造一个pool对象；当关闭http时，销毁pool对象。所以无需释放小块内存。
## 1.2 ngx_slab_t，是有分配与释放内存的动作的。适合频繁的分配和释放内存的操作。
  （１）在slab中，用bitmap，来标记内存是使用状态，还是未使用状态。
  （２）分配的内存，是有2^n这种限制的，可能会浪费内存。

# 2.nginx-push-stream-module-master


