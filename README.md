# Performance-Analysis-CS681

## Performance Measurement of a Web Application

### Overview
This assignment experimentally evaluates the performance of a web application modeled as a
queueing system. An Apache–PHP web server is stressed using both **open-loop** and
**closed-loop** load generators to observe how performance metrics evolve with increasing load.

The experimental results are interpreted using queueing theory concepts such as
**Utilization Law**, **Little’s Law**, and system saturation behavior.

 

### System Setup
- **Server**
  - Apache Web Server
  - PHP enabled
- **Client**
  - `httperf` for open-loop load testing
  - `Tsung` for closed-loop load testing
- **Environment**
  - Client and server connected over the same LAN

 

## Open-Loop Load Testing (httperf)

### Workload Description
- Load generator: `httperf`
- Load model: **Open-loop**
- Total connections per run: 5000
- Arrival rates tested:  
  `200 300 400 500 600 700 800 900 1000 1100 1200 req/s`

### Metrics Collected
- Throughput (requests/sec)
- Response time (avg, min, max)
- Connection time
- Dropped / failed requests
- CPU utilization

### Key Observations
- Throughput increases linearly with arrival rate up to ~300 req/s.
- Response time remains stable in the underloaded region.
- A sharp increase in response time and errors occurs near saturation.
- Severe overload collapse observed around 500 req/s.
- CPU remains close to 100% utilization at high load.

 

## Closed-Loop Load Testing (Tsung)

### Workload Description
- Load generator: `Tsung`
- Load model: **Closed-loop**
- Virtual users repeatedly issue requests to the server
- Think time: ~6 seconds
- Number of users varied from low load to saturation (100+ users)

### Metrics Collected
- Average response time
- Throughput
- CPU utilization
- Failed requests
- Response time vs throughput
- Utilization vs throughput

### Key Observations
- Throughput initially increases with number of users, then flattens.
- Response time increases slowly at first and then grows linearly after saturation.
- A clear throughput plateau identifies system capacity.
- Closed-system Little’s Law holds reasonably well under stable conditions.

 

## Queueing Theory Insights
- **Utilization Law** is used to estimate service time from utilization vs throughput plots.
- **Little’s Law** is validated for both open and closed systems.
- Open-loop experiments show overload collapse behavior.
- Closed-loop experiments demonstrate throughput saturation with linear response-time growth.

 

## Conclusion
This assignment demonstrates how real web servers behave under different workload models.
The experiments validate fundamental queueing laws and highlight the importance of load
characterization (open vs closed) when evaluating system performance.
