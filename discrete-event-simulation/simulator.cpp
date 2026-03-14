#include <iostream>   // allows input/output using cout, cin
#include <queue>      // provides queue and priority_queue data structures
#include <vector>     // provides dynamic array container
#include <cmath>      // math functions (log, min, etc.)
#include <random>     // random number generation utilities
#include <iomanip>    // formatting output

using namespace std;  // allows writing cout instead of std::cout

/* ================= CONFIG ================= */

// Configuration class stores all simulator parameters
class Config
{
public:

    int users = 50;        // number of users in the closed-loop system
    int cores = 4;         // number of CPU cores
    int threads = 100;     // size of thread pool (max concurrent requests)
    int buffer = 500;      // maximum queue capacity for waiting requests

    double service_mean = 0.8;  // mean service time of requests
    double think_mean = 3;      // mean think time of users

    double timeout = 5;         // request timeout threshold

    double quantum = 0.2;       // round robin time slice
    double context_switch = 0.01; // context switching overhead

    double sim_time = 10000;    // total simulation time
    double warmup = 2000;       // warmup time (transient period to ignore)
};

/* ================= EVENT ================= */

// possible event types in the simulator
enum EventType
{
    ARRIVAL,   // user sends a request
    DISPATCH,  // CPU scheduling / service execution
    TIMEOUT    // request timed out
};

// Event class representing events in simulation
class Event
{
public:

    double time;     // event timestamp
    EventType type;  // event type
    int request;     // request id associated with event

    // constructor
    Event(double t, EventType ty, int r)
    {
        time = t;      // set event time
        type = ty;     // set event type
        request = r;   // store request id
    }
};

// comparator used to keep priority_queue sorted by earliest time
class EventCompare
{
public:

    bool operator()(Event a, Event b)
    {
        return a.time > b.time; // smallest time has highest priority
    }
};

/* ================= REQUEST ================= */

// Request class represents a user request
class Request
{
public:

    int id;                    // request id
    double arrival;            // time request arrived
    double remaining_service;  // service time remaining

    bool finished = false;     // whether request finished
    bool timed_out = false;    // whether request exceeded timeout

    // constructor
    Request(int i,double t,double s)
    {
        id=i;                  // assign request id
        arrival=t;             // arrival timestamp
        remaining_service=s;   // initial service time
    }
};

/* ================= METRICS ================= */

// Metrics class stores performance statistics
class Metrics
{
public:

    int goodput=0;     // successfully completed requests
    int badput=0;      // requests completed after timeout
    int dropped=0;     // dropped requests (queue overflow)

    double response_sum=0; // total response time

    double busy_time=0;    // total CPU busy time
};

/* ================= RANDOM ================= */

// Random number generator helper class
class RNG
{
public:

    default_random_engine gen;  // random generator engine

    RNG(int seed)
    {
        gen.seed(seed);         // initialize generator with seed
    }

    // generate exponential random variable
    double exponential(double mean)
    {
        exponential_distribution<double> dist(1.0/mean);
        return dist(gen);
    }

    // generate uniform random number
    double uniform(double a,double b)
    {
        uniform_real_distribution<double> dist(a,b);
        return dist(gen);
    }
};

/* ================= CORE ================= */

// CPU core class
class Core
{
public:

    bool busy=false;          // whether core is currently executing a request
    int current_request=-1;   // id of request being processed
};

/* ================= SIMULATOR ================= */

// Main simulator class
class Simulator
{

public:

    Config cfg;   // simulation configuration

    RNG rng;      // random generator

    double sim_time=0; // current simulation clock

    // event priority queue
    priority_queue<Event,vector<Event>,EventCompare> eventList;

    vector<Request> requests; // list of requests

    vector<Core> cores;       // CPU cores

    queue<int> threadQueue;   // waiting request queue

    Metrics metrics;          // performance metrics

    // constructor
    Simulator(Config c,int seed):cfg(c),rng(seed)
    {
        cores.resize(cfg.cores); // create cores
    }

    /* ================= INITIALIZATION ================= */

    void initialize()
    {

        // generate initial request arrival events
        for(int i=0;i<cfg.users;i++)
        {
            double t=rng.exponential(cfg.think_mean); // user think time

            double service=rng.exponential(cfg.service_mean); // service time

            requests.push_back(Request(i,t,service)); // create request

            eventList.push(Event(t,ARRIVAL,i)); // schedule arrival event
        }

    }

    /* ================= FIND FREE CORE ================= */

    int freeCore()
    {
        // search for idle core
        for(int i=0;i<cores.size();i++)
            if(!cores[i].busy)
                return i;

        return -1; // no free core
    }

    /* ================= ARRIVAL ================= */

    void handleArrival(Event e)
    {

        int id=e.request; // request id

        // generate service time
        requests[id].remaining_service=rng.exponential(cfg.service_mean);

        // record arrival time
        requests[id].arrival=sim_time;

        // if thread pool not full
        if(threadQueue.size()<cfg.threads)
        {
            threadQueue.push(id); // enqueue request
            scheduleDispatch();   // attempt to dispatch to core
        }
        else
        {
            metrics.dropped++; // request dropped due to full queue
        }

    }

    /* ================= DISPATCH ================= */

    void scheduleDispatch()
    {

        int core=freeCore(); // find free CPU core

        if(core==-1) return; // no free core

        if(threadQueue.empty()) return; // no waiting requests

        int req=threadQueue.front(); // get next request
        threadQueue.pop();

        cores[core].busy=true;           // mark core busy
        cores[core].current_request=req; // assign request

        // compute service slice for round-robin
        double slice=min(cfg.quantum,requests[req].remaining_service);

        requests[req].remaining_service-=slice; // reduce remaining service

        metrics.busy_time+=slice; // accumulate CPU busy time

        // schedule next dispatch event after slice
        eventList.push(Event(sim_time+slice+cfg.context_switch,DISPATCH,core));
    }

    /* ================= DISPATCH EVENT ================= */

    void handleDispatch(Event e)
    {

        int core=e.request; // which core triggered event

        int req=cores[core].current_request; // request running on core

        // if request finished
        if(requests[req].remaining_service<=0)
        {

            if(!requests[req].timed_out)
            {
                metrics.goodput++; // successful request

                // ignore warmup period
                if(sim_time>cfg.warmup)
                    metrics.response_sum+=sim_time-requests[req].arrival;
            }
            else
            {
                metrics.badput++; // completed after timeout
            }

            // generate think time before next request
            double think=rng.exponential(cfg.think_mean);

            eventList.push(Event(sim_time+think,ARRIVAL,req));

            cores[core].busy=false; // free CPU
            cores[core].current_request=-1;

        }
        else
        {
            // request not finished -> round robin requeue
            threadQueue.push(req);

            cores[core].busy=false;
            cores[core].current_request=-1;
        }

        scheduleDispatch(); // schedule next request

    }

    /* ================= TIMEOUT ================= */

    void handleTimeout(Event e)
    {
        int id=e.request; // request id
        requests[id].timed_out=true; // mark request timed out
    }

    /* ================= RUN ================= */

    void run()
    {

        // main event processing loop
        while(!eventList.empty())
        {

            Event e=eventList.top(); // get earliest event
            eventList.pop();

            if(e.time>cfg.sim_time) // stop simulation if time exceeded
                break;

            sim_time=e.time; // advance simulation clock

            // process event
            if(e.type==ARRIVAL)
                handleArrival(e);

            else if(e.type==DISPATCH)
                handleDispatch(e);

            else if(e.type==TIMEOUT)
                handleTimeout(e);

        }

    }

};

/* ================= MULTIPLE RUNS ================= */

// perform multiple simulation runs for averaging
Metrics runMultiple(Config cfg,int runs)
{

    Metrics avg;

    for(int i=0;i<runs;i++)
    {

        Simulator sim(cfg,i+1); // create simulator with new seed

        sim.initialize(); // initialize events
        sim.run();        // run simulation

        // accumulate metrics
        avg.goodput+=sim.metrics.goodput;
        avg.badput+=sim.metrics.badput;
        avg.dropped+=sim.metrics.dropped;

        avg.response_sum+=sim.metrics.response_sum;

        avg.busy_time+=sim.metrics.busy_time;
    }

    // compute averages
    avg.goodput/=runs;
    avg.badput/=runs;
    avg.dropped/=runs;

    avg.response_sum/=runs;
    avg.busy_time/=runs;

    return avg;

}

/* ================= EXPERIMENT ================= */

// experiment varying context switching time
void contextSwitchExperiment()
{
    Config cfg;

    cfg.users = 40;
    cfg.threads = 100;

    cout << "ctxswitch,response,throughput\n";

    for(double cs=0.001; cs<=0.1; cs+=0.01)
    {
        cfg.context_switch = cs;

        Metrics m = runMultiple(cfg,20);

        double response = m.response_sum / max(1,m.goodput);

        double throughput = (m.goodput + m.badput) / cfg.sim_time;

        cout << cs << ","
             << response << ","
             << throughput << "\n";
    }
}

/* ================= MAIN ================= */

int main()
{

    //usersExperiment(); // run users experiment if needed
    contextSwitchExperiment(); // run context switching experiment
}