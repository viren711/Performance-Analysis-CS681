#include <iostream>
#include <queue>
#include <vector>
#include <cmath>
#include <random>
#include <iomanip>

using namespace std;

/* ================= CONFIG ================= */

enum ServiceDist{
    UNIFORM_DIST,
    EXP_DIST
};

enum SchedPolicy
{
    FIFO,
    ROUND_ROBIN
};

enum EventType
{
    ARRIVAL,
    DISPATCH,
    TIMEOUT
};


class Config
{
public:

    int users = 50;
    int cores = 4;
    int threads = 100;
    int buffer = 500;

    double service_mean = 0.8;

    double think_min = 2;
    double think_max = 5;

    double timeout = 5;

    double quantum = 0.2;
    double context_switch = 0.01;

    double sim_time = 10000;
    double warmup = 2000;

    ServiceDist service_dist = EXP_DIST;
    SchedPolicy sched_policy = ROUND_ROBIN;
};

/* ================= EVENT ================= */


class Event
{
public:

    double time;
    EventType type;
    int request;

    Event(double t, EventType ty, int r)
    {
        time = t;
        type = ty;
        request = r;
    }
};

class EventCompare
{
public:

    bool operator()(Event a, Event b)
    {
        return a.time > b.time;
    }
};

/* ================= REQUEST ================= */

class Request
{
public:

    int id;
    double arrival;
    double remaining_service;

    bool timed_out=false;

    Request(int i,double t,double s)
    {
        id=i;
        arrival=t;
        remaining_service=s;
    }
};

/* ================= METRICS ================= */

class Metrics
{
public:

    int goodput=0;
    int badput=0;
    int dropped=0;

    double response_sum=0;
    double busy_time=0;
};

/* ================= RANDOM ================= */

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

/* ================= CORE ================= */

class Core
{
public:

    bool busy=false;
    int current_request=-1;
};

/* ================= SIMULATOR ================= */

class Simulator
{

public:

    Config cfg;
    RNG rng;

    double sim_time=0;

    priority_queue<Event,vector<Event>,EventCompare> eventList;

    vector<Request> requests;
    vector<Core> cores;

    queue<int> threadQueue;

    Metrics metrics;

    Simulator(Config c,int seed):cfg(c),rng(seed)
    {
        cores.resize(cfg.cores);
    }

    /* ================= SERVICE DISTRIBUTION ================= */

    double serviceTime()
    {
        if(cfg.service_dist == UNIFORM_DIST)
            return rng.uniform(cfg.service_mean*0.5,
                               cfg.service_mean*1.5);

        return rng.exponential(cfg.service_mean);
    }

    /* ================= THINK TIME ================= */

    double thinkTime()
    {
        return rng.uniform(cfg.think_min,cfg.think_max);
    }

    /* ================= INITIALIZE ================= */

    void initialize()
    {

        for(int i=0;i<cfg.users;i++)
        {
            double t=thinkTime();

            double s=serviceTime();

            requests.push_back(Request(i,t,s));

            eventList.push(Event(t,ARRIVAL,i));
        }

    }

    /* ================= FIND FREE CORE ================= */

    int freeCore()
    {
        for(int i=0;i<cores.size();i++)
            if(!cores[i].busy)
                return i;

        return -1;
    }

    /* ================= ARRIVAL ================= */

    void handleArrival(Event e)
    {

        int id=e.request;

        requests[id].arrival=sim_time;
        requests[id].remaining_service=serviceTime();
        requests[id].timed_out=false;

        if(threadQueue.size()<cfg.threads)
        {
            threadQueue.push(id);

            eventList.push(Event(sim_time+cfg.timeout,TIMEOUT,id));

            scheduleDispatch();
        }
        else
        {
            metrics.dropped++;
        }

    }

    /* ================= DISPATCH ================= */

    void scheduleDispatch()
{

    int core=freeCore();

    if(core==-1) return;
    if(threadQueue.empty()) return;

    int req=threadQueue.front();
    threadQueue.pop();

    cores[core].busy=true;
    cores[core].current_request=req;

    double slice;

    // FIFO → process full request at once
    if(cfg.sched_policy == FIFO)
    {
        slice = requests[req].remaining_service;
        requests[req].remaining_service = 0;
    }
    else
    {
        // ROUND ROBIN
        slice = min(cfg.quantum,
                    requests[req].remaining_service);

        requests[req].remaining_service -= slice;
    }

    metrics.busy_time += slice;

    double load_factor = (double)threadQueue.size() / cfg.cores;

    // prevent zero
    if(load_factor < 1) load_factor = 1;

    double effective_cs = cfg.context_switch * load_factor;

    eventList.push(Event(sim_time + slice + effective_cs, DISPATCH, core));
}

    /* ================= DISPATCH EVENT ================= */

    void handleDispatch(Event e)
    {

        int core=e.request;
        int req=cores[core].current_request;

        if(requests[req].remaining_service<=0)
        {

            if(!requests[req].timed_out)
            {
                metrics.goodput++;

                if(sim_time>cfg.warmup)
                    metrics.response_sum+=sim_time-
                                           requests[req].arrival;
            }
            else
            {
                metrics.badput++;
            }

            double think=thinkTime();

            eventList.push(Event(sim_time+think,
                                 ARRIVAL,req));

            cores[core].busy=false;
            cores[core].current_request=-1;

        }
        else
        {
            // Only requeue in Round Robin
            if(cfg.sched_policy == ROUND_ROBIN)
                threadQueue.push(req);

            cores[core].busy=false;
            cores[core].current_request=-1;
        }

        scheduleDispatch();
    }

    /* ================= TIMEOUT ================= */

    void handleTimeout(Event e)
    {
        int id=e.request;

        requests[id].timed_out=true;
    }

    /* ================= RUN ================= */

    void run()
    {
        while(!eventList.empty())
        {

            Event e=eventList.top();
            eventList.pop();

            if(e.time>cfg.sim_time)
                break;

            sim_time=e.time;

            if(e.type==ARRIVAL)
                handleArrival(e);

            else if(e.type==DISPATCH)
                handleDispatch(e);

            else if(e.type==TIMEOUT)
                handleTimeout(e);

        }

    }

};

/* ================= MULTIPLE RUNS + CI ================= */

void runMultiple(Config cfg,int runs)
{

    vector<double> responses;

    for(int i=0;i<runs;i++)
    {

        Simulator sim(cfg,i+1);

        sim.initialize();
        sim.run();

        double r = sim.metrics.response_sum /
                   max(1,sim.metrics.goodput);

        responses.push_back(r);
    }

    double mean=0;

    for(double x:responses)
        mean+=x;

    mean/=responses.size();

    double var=0;

    for(double x:responses)
        var+=(x-mean)*(x-mean);

    var/=(responses.size()-1);

    double stddev=sqrt(var);

    double CI=1.96*stddev/sqrt(responses.size());

    cout<<"Mean Response = "<<mean<<endl;
    cout<<"95% CI = ["<<mean-CI<<","<<mean+CI<<"]"<<endl;
}

/* ================= EXPERIMENT ================= */

/* ================= USERS EXPERIMENT ================= */


void usersExperiment(Config cfg)
{

    //Config cfg;

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

            double resp = sim.metrics.response_sum /
                          max(1,sim.metrics.goodput);

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

        /* ===== CI CALCULATION ===== */

        double mean=0;

        for(double x:responses)
            mean+=x;

        mean/=responses.size();

        double var=0;

        for(double x:responses)
            var+=(x-mean)*(x-mean);

        var/=(responses.size()-1);

        double stddev=sqrt(var);

        double CI=1.96*stddev/sqrt(responses.size());

        /* ===== METRICS ===== */

        double goodput = avg.goodput / cfg.sim_time;
        double badput  = avg.badput  / cfg.sim_time;

        double throughput = goodput + badput;

        double util = avg.busy_time /
                      (cfg.sim_time * cfg.cores);

        cout<<u<<","
            <<mean<<","
            <<mean-CI<<","
            <<mean+CI<<","
            <<throughput<<","
            <<goodput<<","
            <<badput<<","
            <<avg.dropped<<","
            <<util<<"\n";
    }

}

void contextSwitchExperiment(Config cfg)
{

    //Config cfg;

    cfg.users=40;
    cfg.threads=100;

    cout<<"ctxswitch,response,throughput\n";

    for(double cs=0.001; cs<=0.1; cs+=0.01)
    {

        cfg.context_switch=cs;

        Simulator sim(cfg,1);

        sim.initialize();
        sim.run();

        double response=sim.metrics.response_sum/
                        max(1,sim.metrics.goodput);

        double throughput=(sim.metrics.goodput+
                           sim.metrics.badput)
                           /cfg.sim_time;

        cout<<cs<<","
            <<response<<","
            <<throughput<<"\n";
    }
}

void threadPoolExperiment(Config cfg)
{

    //Config cfg;

    cfg.users = 40;
    cfg.cores = 4;

    cout<<"threads,response,throughput,utilization\n";

    for(int t = 1; t <= 200; t += 10)
    {

        cfg.threads = t;

        vector<double> responses;
        Metrics avg;

        int runs = 20;

        for(int r=0;r<runs;r++)
        {
            Simulator sim(cfg,r+1);

            sim.initialize();
            sim.run();

            double resp = sim.metrics.response_sum /
                          max(1,sim.metrics.goodput);

            responses.push_back(resp);

            avg.goodput += sim.metrics.goodput;
            avg.badput  += sim.metrics.badput;
            avg.busy_time += sim.metrics.busy_time;
        }

        avg.goodput/=runs;
        avg.badput/=runs;
        avg.busy_time/=runs;

        // mean response
        double mean=0;
        for(double x:responses) mean+=x;
        mean/=responses.size();

        double throughput = (avg.goodput + avg.badput)
                            / cfg.sim_time;

        double util = avg.busy_time /
                      (cfg.sim_time * cfg.cores);

        cout<<t<<","
            <<mean<<","
            <<throughput<<","
            <<util<<"\n";
    }

}

/* ================= MAIN ================= */

int main()
{

    Config cfg;

    cfg.service_dist = UNIFORM_DIST;
    cfg.sched_policy = FIFO;

    //usersExperiment(cfg);        // Response Time vs Users + CI
    contextSwitchExperiment(cfg); // Context switch experiment
    //threadPoolExperiment(cfg);
}
