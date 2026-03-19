#include <iostream>
#include <queue>
#include <vector>
#include <cmath>
#include <random>
#include <iomanip>

using namespace std;

/* 
    CONFIGURATION
    */

// Service time distributions supported
enum ServiceDist{
    UNIFORM_DIST,
    EXP_DIST
};

// Scheduling policies
enum SchedPolicy
{
    FIFO,
    ROUND_ROBIN
};

// Event types in simulation
enum EventType
{
    ARRIVAL,
    DISPATCH,
    TIMEOUT
};

// Global configuration of system
class Config
{
public:
    int users = 50;        // number of users (closed system)
    int cores = 4;         // number of CPU cores
    int threads = 100;     // max concurrent threads
    int buffer = 500;      // queue capacity

    double service_mean = 0.8;  // avg service time

    double think_min = 2;       // think time range
    double think_max = 5;

    double timeout = 5;         // request timeout

    double quantum = 0.2;       // RR time slice
    double context_switch = 0.01;

    double sim_time = 10000;    // total simulation time
    double warmup = 2000;       // ignore transient phase

    ServiceDist service_dist = EXP_DIST;
    SchedPolicy sched_policy = ROUND_ROBIN;
};

/* 
                        EVENT SYSTEM
    */

// Event object for DES
class Event
{
public:
    double time;     // event time
    EventType type;  // event type
    int request;     // associated request

    Event(double t, EventType ty, int r)
    {
        time = t;
        type = ty;
        request = r;
    }
};

// Priority queue comparator
class EventCompare
{
public:
    bool operator()(Event a, Event b)
    {
        return a.time > b.time;
    }
};

/* 
                        REQUEST MODEL
    */

class Request
{
public:
    int id;
    double arrival;
    double remaining_service;

    bool timed_out = false;

    Request(int i,double t,double s)
    {
        id=i;
        arrival=t;
        remaining_service=s;
    }
};

/* 
                        METRICS
    */

class Metrics
{
public:
    int goodput=0;   // completed within timeout
    int badput=0;    // completed after timeout
    int dropped=0;   // dropped due to full queue

    double response_sum=0; // sum of response times
    double busy_time=0;    // CPU utilization metric
};

/* 
                        RANDOM GENERATOR
    */

class RNG
{
public:
    default_random_engine gen;

    RNG(int seed)
    {
        gen.seed(seed);
    }

    double exponential(double mean)
    {
        exponential_distribution<double> dist(1.0/mean);
        return dist(gen);
    }

    double uniform(double a,double b)
    {
        uniform_real_distribution<double> dist(a,b);
        return dist(gen);
    }
};

/* 
                        CORE MODEL
    */

class Core
{
public:
    bool busy=false;
    int current_request=-1;
};

/* 
                        SIMULATOR
    */

class Simulator
{
public:
    Config cfg;
    RNG rng;

    double sim_time=0;

    // Event list (priority queue)
    priority_queue<Event,vector<Event>,EventCompare> eventList;

    vector<Request> requests;
    vector<Core> cores;

    queue<int> threadQueue;

    Metrics metrics;

    Simulator(Config c,int seed):cfg(c),rng(seed)
    {
        cores.resize(cfg.cores);
    }

    /*  SERVICE TIME  */

    double serviceTime()
    {
        if(cfg.service_dist == UNIFORM_DIST)
            return rng.uniform(cfg.service_mean*0.5,
                               cfg.service_mean*1.5);

        return rng.exponential(cfg.service_mean);
    }

    /*  THINK TIME  */

    double thinkTime()
    {
        return rng.uniform(cfg.think_min,cfg.think_max);
    }

    /*  INITIALIZATION  */

    void initialize()
    {
        // Create one request per user
        for(int i=0;i<cfg.users;i++)
        {
            double t=thinkTime();
            double s=serviceTime();

            requests.push_back(Request(i,t,s));
            eventList.push(Event(t,ARRIVAL,i));
        }
    }

    /*  CORE ALLOCATION  */

    int freeCore()
    {
        for(int i=0;i<cores.size();i++)
            if(!cores[i].busy)
                return i;

        return -1;
    }

    /*  ARRIVAL EVENT  */

    void handleArrival(Event e)
    {
        int id=e.request;

        requests[id].arrival=sim_time;
        requests[id].remaining_service=serviceTime();
        requests[id].timed_out=false;

        if(threadQueue.size()<cfg.threads)
        {
            threadQueue.push(id);

            // schedule timeout
            eventList.push(Event(sim_time+cfg.timeout,TIMEOUT,id));

            scheduleDispatch();
        }
        else
        {
            metrics.dropped++;
        }
    }

    /*  SCHEDULER  */

    void scheduleDispatch()
    {
        int core = freeCore();
        if(core == -1 || threadQueue.empty()) return;

        // Load-dependent overhead
        double load_factor = (double)threadQueue.size() / cfg.cores;
        if(load_factor < 1) load_factor = 1;

        int req = threadQueue.front();
        threadQueue.pop();

        cores[core].busy = true;
        cores[core].current_request = req;

        double slice;

        // FIFO vs Round Robin execution
        if(cfg.sched_policy == FIFO)
        {
            slice = requests[req].remaining_service;
            requests[req].remaining_service = 0;
        }
        else
        {
            slice = min(cfg.quantum,
                        requests[req].remaining_service);

            requests[req].remaining_service -= slice;
        }

        metrics.busy_time += slice;

        double effective_cs;

        if(cfg.sched_policy == ROUND_ROBIN)
            effective_cs = cfg.context_switch * (1 + load_factor);
        else
            effective_cs = cfg.context_switch;

        eventList.push(Event(sim_time + slice + effective_cs,
                             DISPATCH, core));
    }

    /*  DISPATCH EVENT  */

    void handleDispatch(Event e)
    {
        int core=e.request;
        int req=cores[core].current_request;

        if(requests[req].remaining_service <= 0)
        {
            // Completed request
            if(!requests[req].timed_out)
            {
                if(sim_time > cfg.warmup)
                {
                    metrics.goodput++;
                    metrics.response_sum += sim_time - requests[req].arrival;
                }
            }
            else
            {
                metrics.badput++;
            }

            // Next request after think time
            double think = thinkTime();
            eventList.push(Event(sim_time + think, ARRIVAL, req));

            cores[core].busy = false;
            cores[core].current_request = -1;
        }
        else
        {
            // RR: requeue incomplete request
            if(cfg.sched_policy == ROUND_ROBIN)
                threadQueue.push(req);

            cores[core].busy=false;
            cores[core].current_request=-1;
        }

        scheduleDispatch();
    }

    /*  TIMEOUT  */

    void handleTimeout(Event e)
    {
        requests[e.request].timed_out=true;
    }

    /*  MAIN LOOP  */

    void run()
    {
        while(!eventList.empty())
        {
            Event e=eventList.top();
            eventList.pop();

            if(e.time>cfg.sim_time) break;

            sim_time=e.time;

            if(e.type==ARRIVAL) handleArrival(e);
            else if(e.type==DISPATCH) handleDispatch(e);
            else if(e.type==TIMEOUT) handleTimeout(e);
        }
    }
};

/* 
                        EXPERIMENTS
    */

// Users vs performance (with CI)
void usersExperiment(Config cfg)
{
    cout<<"users,mean_response,ci_low,ci_high,throughput,goodput,badput,drop,utilization\n";

    for(int u=5;u<=100;u+=5)
    {
        cfg.users = u;

        vector<double> responses;
        Metrics avg;

        int runs = 20;

        for(int r=0;r<runs;r++)
        {
            Simulator sim(cfg,r+1);
            sim.initialize();
            sim.run();

            double resp = (sim.metrics.goodput > 0) ?
                          sim.metrics.response_sum / sim.metrics.goodput : 0;

            responses.push_back(resp);

            avg.goodput += sim.metrics.goodput;
            avg.badput  += sim.metrics.badput;
            avg.dropped += sim.metrics.dropped;
            avg.response_sum += sim.metrics.response_sum;
            avg.busy_time += sim.metrics.busy_time;
        }

        avg.goodput/=runs;
        avg.badput/=runs;
        avg.dropped/=runs;
        avg.response_sum/=runs;
        avg.busy_time/=runs;

        // CI calculation
        double mean=0;
        for(double x:responses) mean+=x;
        mean/=responses.size();

        double var=0;
        for(double x:responses) var+=(x-mean)*(x-mean);
        var/=(responses.size()-1);

        double CI=1.96*sqrt(var/responses.size());

        double throughput = (avg.goodput + avg.badput) / cfg.sim_time;
        double util = avg.busy_time / (cfg.sim_time * cfg.cores);

        cout<<u<<","<<mean<<","<<mean-CI<<","<<mean+CI<<","
            <<throughput<<","<<avg.goodput/cfg.sim_time<<","
            <<avg.badput/cfg.sim_time<<","<<avg.dropped<<","<<util<<"\n";
    }
}



/* 
                CONTEXT SWITCH EXPERIMENT
    */

void contextSwitchExperiment(Config cfg)
{
    cout<<"ctx_switch,response,throughput\n";

    cfg.users = 40;  // fixed load

    for(double cs=0.001; cs<=0.1; cs+=0.01)
    {
        cfg.context_switch = cs;

        Metrics avg;
        int runs = 20;

        for(int r=0; r<runs; r++)
        {
            Simulator sim(cfg, r+1);

            sim.initialize();
            sim.run();

            avg.goodput += sim.metrics.goodput;
            avg.badput  += sim.metrics.badput;
            avg.response_sum += sim.metrics.response_sum;
        }

        avg.goodput /= runs;
        avg.badput  /= runs;
        avg.response_sum /= runs;

        double response = avg.response_sum / max(1, avg.goodput);
        double throughput = (avg.goodput + avg.badput) / cfg.sim_time;

        cout<<cs<<","
            <<response<<","
            <<throughput<<"\n";
    }
}

/* 
                THREAD POOL EXPERIMENT
    */

void threadPoolExperiment(Config cfg)
{
    cout<<"threads,response,throughput,utilization\n";

    cfg.users = 40;
    cfg.cores = 4;

    for(int t=1; t<=200; t+=10)
    {
        cfg.threads = t;

        Metrics avg;
        int runs = 20;

        for(int r=0; r<runs; r++)
        {
            Simulator sim(cfg, r+1);

            sim.initialize();
            sim.run();

            avg.goodput += sim.metrics.goodput;
            avg.badput  += sim.metrics.badput;
            avg.response_sum += sim.metrics.response_sum;
            avg.busy_time += sim.metrics.busy_time;
        }

        avg.goodput /= runs;
        avg.badput  /= runs;
        avg.response_sum /= runs;
        avg.busy_time /= runs;

        double response = avg.response_sum / max(1, avg.goodput);
        double throughput = (avg.goodput + avg.badput) / cfg.sim_time;
        double util = avg.busy_time / (cfg.sim_time * cfg.cores);

        cout<<t<<","
            <<response<<","
            <<throughput<<","
            <<util<<"\n";
    }
}

/* 
                        MAIN
    */

int main()
{
    Config cfg;

    cfg.service_dist = EXP_DIST;
    cfg.sched_policy = FIFO;

    usersExperiment(cfg);
}

// int main()
// {
//     int max_users = 100;

//     double S = 0.2;   
//     double Z = 3.5;   

//     vector<double> R(max_users + 1);
//     vector<double> X(max_users + 1);
//     vector<double> Q(max_users + 1);

//     Q[0] = 0;

//     cout << "Users,R_mva,X_mva\n";

//     for(int N = 1; N <= max_users; N++)
//     {
//         R[N] = S * (1 + Q[N-1]);

//         X[N] = N / (R[N] + Z);

//         Q[N] = X[N] * R[N];

//         cout << N << ","
//              << fixed << setprecision(4)
//              << R[N] << ","
//              << X[N] << "\n";
//     }

//     return 0;
// }