#include <iostream>
#include <chrono>
#include "tbb/tbb.h"
#include<unordered_map>
#include<thread>
#include "OptimisticLockCoupling/Tree.h"
#include "ROWEX/Tree.h"
#include "ART/Tree.h"
#include<functional>
#include<omp.h>
#include<algorithm>
#include<map>
#include<vector>
#include <cassert>
#include<thread>
#include<queue>
#include<math.h>
#include <mutex>
#include <condition_variable>
#include <memory>
#include<fstream>
#include<assert.h>
#include<set>

using namespace std::literals::chrono_literals;
using callback = void(*)(void*);
int NUMBER = 2;
#define THREAD_MAX_NUM 1
// std::atomic<uint64_t> sum{0};
//extern int count=0;
using namespace std;
void loadKey(TID tid, Key &key) {
    // Store the key of the tuple into the key vector
    // Implementation is database specific
    key.setKeyLen(sizeof(tid));
    reinterpret_cast<uint64_t *>(&key[0])[0] = __builtin_bswap64(tid);
}

ART_OLC::Tree tree(loadKey);

void read_data_from_file(string &path, uint64_t *result, int n){
    ifstream infile;
    infile.open(path.data(), ios::binary | ios::in);
    if(!infile.is_open()){
        cout<<"open file error"<<endl;
        return;
    }
    uint64_t filesize = infile.seekg(0, ios::end).tellg();
    if(filesize < n * sizeof(uint64_t))
        cout << "file size is not enough" << endl;

    infile.seekg(0, ios::beg);
    infile.read((char*)result, filesize);
    //cout << "read " << infile.gcount() << " bytes" << endl;
    infile.close();
}



void singlethreaded(char **argv) {
    std::cout << "single threaded:" << std::endl;

    uint64_t n = std::atoll(argv[1]);
    uint64_t *keys = new uint64_t[n];

    // Generate keys
    for (uint64_t i = 0; i < n; i++)
        // dense, sorted
        keys[i] = i + 1;
    if (atoi(argv[2]) == 1)
        // dense, random
        std::random_shuffle(keys, keys + n);
    if (atoi(argv[2]) == 2)
        // "pseudo-sparse" (the most-significant leaf bit gets lost)
        for (uint64_t i = 0; i < n; i++)
            keys[i] = (static_cast<uint64_t>(rand()) << 32) | static_cast<uint64_t>(rand());

    //printf("operation,n,ops/s\n");
    ART_unsynchronized::Tree tree(loadKey);
    ART_unsynchronized::Tree tree1(loadKey);
    // Build tree
    {
        auto starttime = std::chrono::system_clock::now();
        for (uint64_t i = 0; i <n/2; i++) {
            Key key;
            loadKey(keys[i], key);
            //key.getall();
            //printf("%d %d\n",keys[i],key.getKeyLen());
            tree.insert(key, keys[i]);
        }
        for (uint64_t i = n/2; i <n; i++) {
            Key key;
            loadKey(keys[i], key);
            //key.getall();
            //printf("%d %d\n",keys[i],key.getKeyLen());
            tree1.insert(key, keys[i]);
        }
        auto endtime = std::chrono::system_clock::now();
        cout<<"insert, "<<n<<" "<<std::chrono::duration_cast<std::chrono::milliseconds>(endtime-starttime).count()<<"ms"<<endl;
        //auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        //        std::chrono::system_clock::now() - starttime);
        //printf("insert,%ld,%f\n", n, (n * 1.0) / duration.count());
    }

    {
        // Lookup
        auto starttime = std::chrono::system_clock::now();
        for (uint64_t i = 0; i != n; i++) {
            Key key;
            loadKey(keys[i], key);
            auto val = tree.lookup(key);
            auto val1 = tree1.lookup(key);
            if (val != keys[i]&&val1!=keys[i]) {
                std::cout << "wrong key read: " << val << " expected:" << keys[i] << std::endl;
                throw;
            }
        }
        //auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        //    std::chrono::system_clock::now() - starttime);
        //printf("lookup,%ld,%f\n", n, (n * 1.0) / duration.count());
        auto endtime = std::chrono::system_clock::now();
        cout<<"lookup, "<<n<<" "<<std::chrono::duration_cast<std::chrono::milliseconds>(endtime-starttime).count()<<"ms"<<endl;
    }

    {
        auto starttime = std::chrono::system_clock::now();

        for (uint64_t i = 0; i <n/2; i++) {
            Key key;
            loadKey(keys[i], key);
            tree.remove(key, keys[i]);
        }
        for (uint64_t i = n/2; i != n; i++) {
            Key key;
            loadKey(keys[i], key);
            tree1.remove(key, keys[i]);
        }
        auto endtime = std::chrono::system_clock::now();
        cout<<"remove, "<<n<<" "<<std::chrono::duration_cast<std::chrono::milliseconds>(endtime-starttime).count()<<"ms"<<endl;
        // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        //        std::chrono::system_clock::now() - starttime);
        // printf("remove,%ld,%f\n", n, (n * 1.0) / duration.count());
    }
    delete[] keys;

    std::cout << std::endl;
}

void multithreaded_ART_OLC(char **argv) {
    std::cout << "multi threaded ART_OLC:" << std::endl;

    uint64_t n = std::atoll(argv[1]);
    uint64_t *keys = new uint64_t[n];

    //uint64_t *lookkeys = new uint64_t[2*n];
    //std::random_shuffle(lookkeys, lookkeys + 2*n);

    // Generate keys
    for (uint64_t i = 0; i < n; i++)
        // dense, sorted
        keys[i] = i + 1;
    if (atoi(argv[2]) == 1)
        // dense, random
        std::random_shuffle(keys, keys + n);
    if (atoi(argv[2]) == 2)
        // "pseudo-sparse" (the most-significant leaf bit gets lost)
        for (uint64_t i = 0; i < n; i++)
            keys[i] = (static_cast<uint64_t>(rand()) << 32) | static_cast<uint64_t>(rand());

  
    ART_OLC::Tree tree(loadKey);
    tbb::task_scheduler_init init(atoi(argv[3]));
    // for(int i=0;i<(512000/2)*1;i++)
    // {
    //     int index1=rand()%5120000;
    //     int index2=rand()%5120000;
    //     // cout<<index1<<" "<<index2<<endl;
    //     swap(keys[index1],keys[index2]);
    // }
    // for(int i=0;i<(256/2)*1;i++)
    // {
    //     int index1=rand()%256;
    //     int index2=rand()%256;
    //     swap(keys[index1],keys[index2]);
    // }
    // int blocksize=n/10000;
    // for(int i=0;i<n;i++)
    // {
    //     uint64_t tag=keys[i]/256;
    //     keys[i]+=(tag<<24);
    // }
    // for(int i=0;i<n/2;i++)
    // {
    //     // int index1=rand()%256;
    //     keys[i]=rand();
    // }
    {
        int count[16]={0};
        int sum=0;
        auto starttime = std::chrono::system_clock::now();
        // for(int j=1;j<10000;j++)
        // {
            // int amax=0;
            auto t0 = std::chrono::system_clock::now();
            tbb::parallel_for(tbb::blocked_range<uint64_t>(0,n), [&](const tbb::blocked_range<uint64_t> &range) {
                auto t = tree.getThreadInfo();
                for (uint64_t i = range.begin(); i != range.end(); i++) {
                    // cout<<range.end()-range.begin()<<endl;
                    Key key;
                    loadKey(keys[i], key);
                    tree.insert(key, keys[i], t, count[tbb::task_arena::current_thread_index()]);
                }
            });
            for(int i=0;i<16;i++)cout<<count[i]<<endl,sum+=count[i];
            cout<<sum<<endl;
            auto t1 = std::chrono::system_clock::now();
            // cout<<"insert, "<<n<<" "<<std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count()<<"us"<<endl;
        auto endtime = std::chrono::system_clock::now();
        cout<<"insert, "<<n<<" "<<std::chrono::duration_cast<std::chrono::microseconds>(endtime-starttime).count()<<"us"<<endl;
    }

    {
        // Lookup
        auto starttime = std::chrono::system_clock::now();
        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            auto t = tree.getThreadInfo();
            for (uint64_t i = range.begin(); i != range.end(); i++) {
                Key key;
                loadKey(keys[i], key);
                auto val = tree.lookup(key, t);
                //printf("%d",i);
                if (val != keys[i]) {
                    std::cout << "wrong key read: " << val << " expected:" << keys[i] << std::endl;
                    throw;
                }
            }

        });
        auto endtime = std::chrono::system_clock::now();
        cout<<"lookup, "<<n<<" "<<std::chrono::duration_cast<std::chrono::microseconds>(endtime-starttime).count()<<"us"<<endl;
    }

    {
        auto starttime = std::chrono::system_clock::now();

        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            auto t = tree.getThreadInfo();
            for (uint64_t i = range.begin(); i != range.end(); i++) {
                Key key;
                loadKey(keys[i], key);
                tree.remove(key, keys[i], t);
            }
        });
        auto endtime = std::chrono::system_clock::now();
        cout<<"remove, "<<n<<" "<<std::chrono::duration_cast<std::chrono::microseconds>(endtime-starttime).count()<<"us"<<endl;
    }
    delete[] keys;
}


class ThreadPool {
private:
    bool m_open_flag;                     // 表示线程池运行标志，用于线程回收
                  // 线程池中剩余线程数，用于检测线程退出

    mutex m_mutex_run;                    // 用于线程运行时互斥，主要是互斥操作任务队列
    mutex m_mutex_quit;                   // 用于线程退出时互斥
    condition_variable m_cond_run;        // 用于线程间运行时同步
    condition_variable m_cond_quit;       // 用于线程间退出时同步

    queue<function<void()>> m_task_queue; // 线程池任务队列

public:
    int m_thread_count;     
    explicit ThreadPool(int thread_count = THREAD_MAX_NUM) {
        assert(thread_count > 0);

        m_open_flag = true;
        m_thread_count = thread_count;

        for (int i = 0; i < thread_count; i++) {
            /* 通过匿名函数依次创建子线程 */
            thread([this] {
                unique_lock<mutex> locker(m_mutex_run); // 互斥操作任务队列
                while (true) {
                    if (!m_task_queue.empty()) {
                        auto task = std::move(m_task_queue.front()); // 通过move将任务函数转换为右值引用，提高传递效率
                        m_task_queue.pop();
                        locker.unlock(); // 把锁释放，避免自己的业务处理影响其他线程的正常执行
                        task(); // 执行任务队列中的任务函数
                        locker.lock(); // 重新获取锁
                    } else if (!m_open_flag) {
                        m_thread_count--;
                        if (m_thread_count == 0) {
                            m_cond_quit.notify_one(); // 所有子线程均已退出，通知主线程退出
                        }
                        break;
                    } else {
                        m_cond_run.wait(locker); // 阻塞等待 m_mutex_run
                    }
                }
            }).detach();
        }
    }

    ~ThreadPool() {
        {
            unique_lock<mutex> locker(m_mutex_run); // 互斥操作m_open_flag
            m_open_flag = false;
        }
        m_cond_run.notify_all(); // 通知线程队列中的所有子线程退出
        {
            unique_lock<mutex> locker(m_mutex_quit);
            m_cond_quit.wait(locker); // 阻塞等待m_mutex_quit，会由最后一个退出的子线程通知
        }
    }

    template<class T>
    void addTask(T &&task) {
        {
            // if(m_task_queue.size>=2)
            unique_lock<mutex> locker(m_mutex_run); // 互斥操作任务队列
            m_task_queue.emplace(std::forward<T>(task)); // 通过完美转发传递任务函数
        }
        m_cond_run.notify_one(); // 通知线程队列中的首子线程执行任务
    }
};

bool flag=true;
int64_t amax=0;
void thread_function(vector<uint64_t> *bucket,int index)
{
    //printf("test\n");
    std::thread::id this_id = std::this_thread::get_id();
    // ofstream outfile;
    // outfile.open("out.txt",ios::out);
    //out << "test";
    while(flag){std::this_thread::sleep_for(std::chrono::nanoseconds(1));};
    
    
    //Cache c;
    //c.cache_node=NULL; 
    int n=bucket->size();
    //int levelcount[8]={0};
    int64_t sum=0;
    // auto t = tree.getThreadInfo();
    auto starttime = std::chrono::system_clock::now();
    
    for(int i=0;i<n;i++)
    {
        // auto t0 = std::chrono::system_clock::now();
        Key key;
        loadKey((*bucket)[i], key);        
        // tree.insert_lockfree(key, (*bucket)[i], t);
        tree.insert_level(key, (*bucket)[i],0);
        // auto t1 = std::chrono::system_clock::now();
        // if(index==10||index==4)
        // {
        //     outfile<<"thread_id: "<<index<<", key: "<<(*bucket)[i]<<", time: "<<std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count()<<"us"<<endl;
        // }
        
        // if(std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count()>10)
        // {
        //     printf("thread_id: %d, key: %lx, time: %dus\n",index,(*bucket)[i],std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count());
        // }
        // sum+=std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    }
    
    auto endtime = std::chrono::system_clock::now();
    // outfile.close();
    // if((*bucket)[0]==0x643c986966334873)
    // {
    //     for(int i=0;i<n;i++)printf("%lx\n",(*bucket)[i]);
    // }
    // printf("bucket: %d,size: %d,time: %dus\n",index,bucket->size(),std::chrono::duration_cast<std::chrono::microseconds>(endtime-starttime).count());
    
    // amax=max(amax,sum);
    amax=max(amax,std::chrono::duration_cast<std::chrono::microseconds>(endtime-starttime).count());
    // amax+=std::chrono::duration_cast<std::chrono::microseconds>(endtime-starttime).count();
}



struct block
{
    // int index;
    vector<uint64_t> val;
    uint64_t data[256];
    int count;
    Cache c;
    uint64_t label;
    // struct block* next;
};


void thread_fun(block* p)
{
    int n=p->val.size();
    int time=0;
    Key key;
    auto t0 = std::chrono::system_clock::now();
    for(int i=0;i<n;i++)
    {
        loadKey(p->val[i], key);
        tree.insert(key, p->val[i],p->c, time);
        // if(i==n-1)
        // {
        //     if(p->c.cache_node!=NULL)
        //     p->c.cache_node->writeUnlockObsolete();
        // }
    }
}

void thread_fun_small(block* p)
{
    auto t = tree.getThreadInfo();
    int n = p->val.size();
    Key key;
    for(int i=0;i<n;i++)
    {
        loadKey(p->val[i], key);
        // tree.insert(key, p->val[i], t );
    }
}

void test(char **argv)
{
    uint64_t n = std::atoll(argv[1]);
    uint64_t *keys = new uint64_t[n];


    // Generate keys
    for (uint64_t i = 0; i < n; i++)
        // dense, sorted
        keys[i] = i;
    if (atoi(argv[2]) == 1)
        // dense, random
        {
            std::random_shuffle(keys, keys + n);
            for(int i=0;i<n;i++)
            {
                keys[i]=keys[i]<<16;
            }
        }
    if (atoi(argv[2]) == 2)
    {
        for (uint64_t i = 0; i < n; i++)
        {
            keys[i] = (static_cast<uint64_t>(rand()) << 32) | static_cast<uint64_t>(rand());
        }
    }
    
    tbb::task_scheduler_init init(atoi(argv[3]));

    int sumtime=0;
    int64_t sumparttime=0;
    // int blocksize=n/10000;

    // 打乱顺序
    // for(int i=0;i<(256/2)*1;i++)
    // {
    //     int index1=rand()%256;
    //     int index2=rand()%256;
    //     swap(keys[index1],keys[index2]);
    // }
    // for(int i=0;i<256;i++)
    // {
    //     // int index1=rand()%256;
    //     keys[i]=rand();
    // }
    // for(int i=0;i<256;i++)
    // {
    //     uint64_t tag=i/32;
    //     keys[i]=keys[i]+(tag<<56);
    // }
    // ThreadPool thread_pool;
    // for(int i=0;i<n;i++)
    // {
    //     uint64_t tag=keys[i]/256;
    //     keys[i]+=(tag<<24);
    // }
    {
        auto t = tree.getThreadInfo();
        // uint64_t buckets[16][100000];
        // int count[16]={0};
        // bool *active=new bool[n];
        bool flag = true;
        // for(int i=0;i<n;i++)active[i]=true;
        priority_queue<uint64_t> q;  
        auto starttime = std::chrono::system_clock::now();
        // for(int i=0;i<n;i++)
        // {
        //     // printf("%x\n",((keys[i]&(0x0F000000))>>24));
        //     buckets[(keys[i]&(0x0F000000))>>24][count[(keys[i]&(0x0F000000))>>24]]=keys[i];
        //     count[(keys[i]&(0x0F000000))>>24]++;
        // }
        // for(int i=0;i<16;i++)
        // {
        //     cout<<count[i]<<endl;
        // }
        // # pragma omp parallel num_threads(16)
        // for(int i=0;i<16;i++)
        // {
            // for(int i=0;j<n;j++)
            // {
            //     Key key;
            //     loadKey(keys[i], key);
            //     tree.insert_lockfree(key, keys[i]);
            // }
        // }
        //分块插入
        // unordered_map<uint64_t,block*>mp;  
        // auto t0 = std::chrono::system_clock::now();
        // for(int i=0;i<n;i++)
        // {
        //     uint64_t prefix=keys[i]&(0xFFFFFFFFFFFFFF00);
        //     /*直接分块
        //     // int j;
        //     // for(j=0;j<bucketcount;j++)
        //     // {
        //     //     if(prefix==data[j].label)
        //     //     {
        //     //         data[j].val.push_back(keys[i]);
        //     //         break;
        //     //     }
        //     // }
        //     // if(j==bucketcount)
        //     // {
        //     //     data[j].label=prefix;
        //     //     data[j].val.push_back(keys[i]);
        //     //     bucketcount++;
        //     // }
        //     */
            
        //     //哈希分块
        //     if(mp.find(prefix)==mp.end())
        //     {
        //         block* p=new block();
        //         // p->c.cache_node=NULL;
        //         mp[prefix]=p;
        //         // p->val.push_back(keys[i]);
        //         p->count=1;
        //         p->data[0]=keys[i];
        //     }
        //     else 
        //     {
        //         // mp[prefix]->val.push_back(keys[i]);
        //         mp[prefix]->data[mp[prefix]->count]=keys[i];
        //         mp[prefix]->count++;
        //     }
        // }
        // auto t1 = std::chrono::system_clock::now();
        // // cout<<mp.size()<<endl;
        // for(auto p:mp)
        // {
        //     // cout<<p.second->count<<endl;
        //     tree.insert(p.second->data, t, p.second->count);
        //     // cout<<11<<endl;
        // }

        //挨个查找插入
        // int i=0;
        // while(flag)
        // {
        //     flag=false;
        //     uint64_t curtag;
        //     int times=0;
        //     for(;i<n;i++)
        //     {
        //         if(active[i])
        //         {
        //             // active[i]=false;
        //             flag=true;
        //             curtag=keys[i]&(0xFFFFFFFFFFFFFF00);
        //             // printf("active[i]=%d\n",active[i]);
        //             break;
        //         }
        //     }
        //     tree.insert(keys,t,curtag,active,n,i,times);
        //     if(times<5)
        //     {
        //         int j=i+5;
        //         Key key;
        //         while(i<n&&i<j)
        //         {
        //             if(active[i])
        //             {
        //                 loadKey(keys[i], key);
        //                 tree.insert(key, keys[i], t, times);
        //                 // q.push(keys[i]);
        //                 active[i]=false;
        //             }
        //             i++;
        //         }
        //     }
        // }
        //到这为止

        /*
        // bucket *data=new bucket[256];
        // int bucketcount=0;
        // for(int i=0;i<n;i++)
        // {
        //     uint64_t prefix=keys[i]&(0xFFFFFFFFFFFFFF00);
        //     int j;
        //     for(j=0;j<bucketcount;j++)
        //     {
        //         if(prefix==data[j].label)
        //         {
        //             data[j].val[data[j].count]=(keys[i]);
        //             data[j].count++;
        //             break;
        //         }
        //     }
        //     if(j==bucketcount)
        //     {
        //         data[j].label=prefix;
        //         data[j].count=1;
        //         data[j].val[0]=(keys[i]);

        //         bucketcount++;
        //     }
        // }
        // for(int i=0;i<bucketcount;i++)
        // {
        //     tree.insert(&data[i],t);
        //     // cout<<11<<endl;
        // }
        //  10000批插入
        // unordered_map<uint64_t,block*>mp[10000];  
        // for(int j=0;j<10000;j++)
        // {
        //     //分桶
        //     // cout<<j<<endl;
            
        //     auto t0 = std::chrono::system_clock::now();
        //     // block *data=new block[256];
        //     int bucketcount=0;
        //     for(int i=blocksize*j;i<blocksize*(j+1);i++)
        //     {
        //         uint64_t prefix=keys[i]&(0xFFFFFFFFFFFFFF00);
        //         // int j;
        //         // for(j=0;j<bucketcount;j++)
        //         // {
        //         //     if(prefix==data[j].label)
        //         //     {
        //         //         data[j].val.push_back(keys[i]);
        //         //         break;
        //         //     }
        //         // }
        //         // if(j==bucketcount)
        //         // {
        //         //     data[j].label=prefix;
        //         //     data[j].val.push_back(keys[i]);
        //         //     bucketcount++;
        //         // }
        //         if(mp[j].find(prefix)==mp[j].end())
        //         {
        //             block* p=new block();
        //             p->c.cache_node=NULL;
        //             mp[j][prefix]=p;
        //             p->val.push_back(keys[i]);
        //         }
        //         else 
        //         {
        //             mp[j][prefix]->val.push_back(keys[i]);
        //         }
        //     }
        //     auto t1 = std::chrono::system_clock::now();
        //     sum+=std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
        //     // cout<<sum<<endl;
        //     // int count=0;
        //     for(auto &p:mp[j])  //划分给多个线程进行插入
        //     {
        //         int n=p.second->val.size();
        //         // cout<<n<<endl;
        //         if(n>48)//插入在同一个节点中的256个数据  生成找到并生成N256  缓存其地址
        //         {
        //             //大块生成线程插入
        //             // mythread[count]=new thread(thread_fun,p.second,count);
        //             // count++;
        //             // cout<<p.second->val.size()<<endl;
                    
        //             // thread_pool.addTask(bind(thread_fun, p.second));
        //             //由main函数插入
        //             int time=0;
        //             Key key;
                    
        //             for(int i=0;i<n;i++)
        //             {
                        
        //                 loadKey(p.second->val[i], key);
        //                 tree.insert(key, p.second->val[i],p.second->c, time);
        //             }
        //         }
        //         else //让一个线程进行有锁插入
        //         {
        //             for(int i=0;i<n;i++)
        //             {
        //                 Key key;
        //                 loadKey(p.second->val[i], key);
        //                 tree.insert(key, p.second->val[i], t );
        //             }
        //         }
        //     }
        //     // auto t1 = std::chrono::system_clock::now();
        //     // cout<<std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count()<<endl;
        // //     //多线程join            
        // //     // for(int i=0;i<2;i++)
        // //     // {
        // //     //     mythread[i]->join();
        // //     // }
        // //     // while(thread_pool.m_thread_count!=2)std::this_thread::sleep_for(std::chrono::nanoseconds(1));
        // //     // cout<<thread_pool.m_thread_count<<endl;
        // //     // while(thread_pool.m_thread_count!=1){
        // //         // cout<<thread_pool.m_thread_count<<endl;
        // //         // std::this_thread::sleep_for(std::chrono::nanoseconds(1));
        // //     // }
            

        // }
        */
        auto endtime = std::chrono::system_clock::now();
        // cout<<sum<<endl;
        // cout<<"participating, "<<std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count()<<"us"<<endl;
        cout<<"insert, "<<n<<" "<<std::chrono::duration_cast<std::chrono::microseconds>(endtime-starttime).count()<<"us"<<endl;
    }
    // std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // cout<<sum<<endl;
    {
        // Lookup
        auto starttime = std::chrono::system_clock::now();
        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            auto t = tree.getThreadInfo();
            for (uint64_t i = range.begin(); i != range.end(); i++) {
                Key key;
                loadKey(keys[i], key);
                auto val = tree.lookup(key, t);
                //printf("%d",i);
                if (val != keys[i]) {
                    // cout<<keys[i]<<endl;
                    std::cout << "wrong key read: " << val << " expected:" << keys[i] << std::endl;
                    throw;
                }
            }

        });
        auto endtime = std::chrono::system_clock::now();
        cout<<"lookup, "<<n<<" "<<std::chrono::duration_cast<std::chrono::microseconds>(endtime-starttime).count()<<"us"<<endl;
    }

    {
        auto starttime = std::chrono::system_clock::now();

        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            auto t = tree.getThreadInfo();
            for (uint64_t i = range.begin(); i != range.end(); i++) {
                Key key;
                loadKey(keys[i], key);
                tree.remove(key, keys[i], t);
            }
        });
        auto endtime = std::chrono::system_clock::now();
        cout<<"remove, "<<n<<" "<<std::chrono::duration_cast<std::chrono::microseconds>(endtime-starttime).count()<<"us"<<endl;
    }

    delete[] keys;
}


int main(int argc, char **argv) {
    if (argc != 4) {
        printf("usage: %s n 0|1|2 线程数 test需要自己修改代码 原版直接输入\nn: number of keys\n0: sorted keys\n1: dense keys\n2: sparse keys\n样例 ./example 5120000 0 1", argv[0]);
        return 1;
    }
    // test(argv);   //分桶版

    singlethreaded(argv);

    // multithreaded_ART_OLC(argv);   //原版

    return 0;
}
