repositório para rinha de backend 2024

WIP

rinha de backend 2024 requirements: 
    - make a debit/crebit system
    - the data must be stored into disk
    - there should be at least two instances of the debit/credit API
    - there should be a round-robin load balancer between those two APIs
    - the components should run in docker-compose
    - i am doing everything in C

so far:
    - a basic single c file of the system (without load balancer, only one API instance) is done

    todo:
    - separate the API from the DB file
    - create another API instance
    - create load balancer
    - dockerize everything
    - ???
    - profit