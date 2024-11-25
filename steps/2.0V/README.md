2.0 使用多线程,并完成wrk测试
```bash
yxqf@yxqf-virtual-machine:~/桌面$ wrk -c100 -d3s http://127.0.0.1:8090
Running 3s test @ http://127.0.0.1:8090
  2 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    41.53ms    3.61ms  70.77ms   96.12%
    Req/Sec     1.20k    75.64     1.36k    81.67%
  7165 requests in 3.01s, 0.93MB read
Requests/sec:   2382.56
Transfer/sec:    317.89KB
yxqf@yxqf-virtual-machine:~/桌面$ S

```
