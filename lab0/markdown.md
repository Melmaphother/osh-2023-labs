> 一个我一直觉得很有意思的公平洗牌算法

```c++
    for(int i = n - 1;i >= 0;i--){
        swap(arr[i] ,arr[rand()%(i + 1)]);
    }
```
我永远喜欢胡桃
![hutao]()
