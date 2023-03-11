- 一个我一直觉得很有意思的公平洗牌算法

```c++
    for(int i = n - 1;i >= 0;i--){
        swap(arr[i] ,arr[rand()%(i + 1)]);
    }
```

- 我永远喜欢胡桃
  ![hutao](https://github.com/Melmaphother/OSH-2023-labs/blob/main/lab0/src/hutao.jpg)

- 公式 
  
  $$
  \nabla \times E = - \frac{\partial B}{\partial t}  \tag{1}
  $$
  
  $$
  \nabla \times H = J + \frac{\partial D}{\partial t}   \tag{2}
  $$
  
  $$
  \nabla \cdot D = \rho  \tag{3}
  $$
  
  $$
  \nabla \cdot B = 0   \tag{4}
  $$
