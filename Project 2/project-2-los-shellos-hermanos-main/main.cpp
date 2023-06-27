#include <iostream>
#include <cstdlib>
#include <ctime>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include "sleep.hh"
#include <vector>
#include <fstream> //added for keepers log part

//Yiğithan Şanlik 64117
//Furkan Tuna 69730

std::ofstream voter_log;  // Global fstream object for Keeps Log Part

double simulation_time = 0;
double probability = 0;
double failure_probability = 0;
time_t start_time;

//this Struct contains fields for tracking ## of votes of candidates/variables
struct PollingStation {
    pthread_mutex_t mutex;
    pthread_cond_t next_voter_cond;
    pthread_cond_t mechanic_cond;
    unsigned int queue_number;
    unsigned int next_voter;
    int id;
    bool is_failed;
    int votes_mary;
    int votes_john;
    int votes_anna;
};

//Struct to store info related to voter
struct VoterInfo {
    unsigned int voter_number;
    bool priority;
    PollingStation *polling_station;
};
//For Keeps Log part to keep the log in details
struct VoterLogInfo {
    int station_id;
    unsigned int voter_id;
    char category;
    time_t request_time;
    time_t polling_station_time;
};


double generate_random_probability() {
    return static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
}
//simulates casting
//Helps us to monitor voting and increments the vote count for candidates
void cast_vote(PollingStation *polling_station) {
    double p = generate_random_probability();
    if (p <= 0.4) {
        polling_station->votes_mary++;
    } else if (p > 0.4 && p <= 0.55) {
        polling_station->votes_john++;
    } else {
        polling_station->votes_anna++;
    }
}

//In general defition, we implemented whole voter process in here. Actions of Waiting/Casting
void *voter_process(void *arg) {
    VoterInfo *voter_info = (VoterInfo *)arg;
    unsigned int voter_number = voter_info->voter_number;
    bool priority = voter_info->priority;
    PollingStation *polling_station = voter_info->polling_station;

    //For Keeps Log Part - To record the current time when a voter requests their queue position
    time_t request_time, polling_station_time;
    time(&request_time);

    static int non_priority_wait_count = 0;

    pthread_mutex_lock(&polling_station->mutex);
    while (voter_number != polling_station->next_voter || (!priority && non_priority_wait_count >= 5) || polling_station->is_failed) {
        pthread_cond_wait(&polling_station->next_voter_cond, &polling_station->mutex);
    }

    if (priority) {
        non_priority_wait_count = 0;
    } else {
        non_priority_wait_count++;
        if (non_priority_wait_count >= 5) {
            non_priority_wait_count = 0;
        }
    }

    int vote_before = polling_station->votes_mary + polling_station->votes_john + polling_station->votes_anna;
    cast_vote(polling_station);
    int vote_after = polling_station->votes_mary + polling_station->votes_john + polling_station->votes_anna;
    if (vote_after > vote_before) {
        time_t current_time;
        time(&current_time);
        std::cout << "At " << difftime(current_time, start_time) << " sec, polling station " << polling_station->id;
        if (priority) {
            std::cout << ", elderly/pregnant: ";
        } else {
            std::cout << ", ordinary: ";
        }
        std::cout << voter_number << std::endl;
    }
    pthread_sleep(2);

    // After the vote is cast and the thread is about to finish
    time(&polling_station_time);
    VoterLogInfo log_info = {
        polling_station->id,
        voter_number,
        priority ? 'S' : 'O',
        request_time,
        polling_station_time
    };

    // Write log_info to voters.log
    voter_log << log_info.station_id << "." << log_info.voter_id << " "
              << log_info.category << " " << difftime(log_info.request_time, start_time) << " "
              << difftime(log_info.polling_station_time, start_time) << " "
              << difftime(log_info.polling_station_time, log_info.request_time) << std::endl;

    polling_station->next_voter++;
    pthread_cond_broadcast(&polling_station->next_voter_cond);
    pthread_mutex_unlock(&polling_station->mutex);

    delete voter_info;
    return NULL;
}

//This function checks if the polling station has failed. (we can change its probability of failing by -f when we run)(Implemented it in Part III)
//If its failed this function fixes it
void *mechanic_process(void *arg) {
    PollingStation *polling_station = (PollingStation *)arg;
    while (true) {
        pthread_testcancel();
        pthread_mutex_lock(&polling_station->mutex);
        if (generate_random_probability() < failure_probability) {
            polling_station->is_failed = true;
            std::cout << "Polling station " << polling_station->id << " has failed." << std::endl;
            pthread_cond_broadcast(&polling_station->next_voter_cond);
        }
        pthread_mutex_unlock(&polling_station->mutex);
        sleep(10);
        if (polling_station->is_failed) {
            pthread_mutex_lock(&polling_station->mutex);
            std::cout << "Mechanic fixing polling station " << polling_station->id << std::endl;
            sleep(5);
            polling_station->is_failed = false;
            std::cout << "Polling station " << polling_station->id << " is fixed." << std::endl;
            pthread_cond_broadcast(&polling_station->mechanic_cond);
            pthread_mutex_unlock(&polling_station->mutex);
        }
    }
    return NULL;
}
void *display_votes(void *arg) {
    std::vector<PollingStation> *polling_stations = (std::vector<PollingStation> *)arg;
    while (true) {
        sleep(20);
        int total_votes_mary = 0;
        int total_votes_john = 0;
        int total_votes_anna = 0;
        for (PollingStation &station : *polling_stations) {
            total_votes_mary += station.votes_mary;
            total_votes_john += station.votes_john;
            total_votes_anna += station.votes_anna;
        }
        std::cout << "At 20 sec total votes: (Mary: " << total_votes_mary << ", John: " << total_votes_john << ", Anna: " << total_votes_anna << ")" << std::endl;
    }
    return NULL;
}

//Changed if condition inside of generate_voters for PART2.
//Added PollingStation and Polling_stations for Part IV
void generate_voters(std::vector<pthread_t>& voter_threads, std::vector<PollingStation>& polling_stations) {
    time_t start_time, current_time;
    time(&start_time);


    while (true) {
        time(&current_time);
        if (difftime(current_time, start_time) >= simulation_time) {
            break;
        }

        bool is_priority_voter = generate_random_probability() > probability;
	    PollingStation *least_busy_station = &polling_stations[0];
        for (PollingStation &station : polling_stations) {
            if (station.queue_number - station.next_voter < least_busy_station->queue_number - least_busy_station->next_voter) {
                least_busy_station = &station;
            }
        }

        pthread_t voter_thread;
        VoterInfo *current_voter_info = new VoterInfo{least_busy_station->queue_number, is_priority_voter, least_busy_station};
	pthread_create(&voter_thread, NULL, voter_process, current_voter_info);
        voter_threads.push_back(voter_thread);

	least_busy_station->queue_number++;
	sleep(1);

    }
}


int main(int argc, char **argv) {

	//Open voters.log file
	voter_log.open("voters.log", std::ios::out | std::ios::app);

    srand(time(NULL));

    int num_polling_stations = 1;
    int opt;
    while ((opt = getopt(argc, argv, "t:p:c:f:")) != -1) {
        switch (opt) {
            case 't':
                simulation_time = atoi(optarg);
                break;
            case 'p':
                probability = atof(optarg);
                break;
	    case 'c': //PART IV
                num_polling_stations = atoi(optarg);
                break;
	    case 'f': //PART III
                failure_probability = atof(optarg);
                break;
                default:
                std::cerr << "Usage: " << argv[0] << " -t simulation_time -p probability -c num_polling_stations -f failure_probability" << std::endl;
                exit(EXIT_FAILURE);
        }
    }
	std::vector<PollingStation> polling_stations(num_polling_stations);
    std::vector<pthread_t> mechanic_threads(num_polling_stations);
    for (int i = 0; i < num_polling_stations; i++) {
        polling_stations[i].id = i;
        polling_stations[i].queue_number = 0;
        polling_stations[i].next_voter = 0;
        polling_stations[i].is_failed = false;
        polling_stations[i].votes_mary = 0;
        polling_stations[i].votes_john = 0;
        polling_stations[i].votes_anna = 0;
        pthread_mutex_init(&polling_stations[i].mutex, NULL);
        pthread_cond_init(&polling_stations[i].next_voter_cond, NULL);
        pthread_cond_init(&polling_stations[i].mechanic_cond, NULL);
        // Create mechanic threads
        pthread_create(&mechanic_threads[i], NULL, mechanic_process, &polling_stations[i]);
    }
    pthread_t display_votes_thread;
    pthread_create(&display_votes_thread, NULL, display_votes, &polling_stations);


    std::vector<pthread_t> voter_threads;
    generate_voters(voter_threads, polling_stations);

    for (size_t i = 0; i < voter_threads.size(); i++) {
	pthread_join(voter_threads[i], NULL);
	}
	 // Cancel and join mechanic threads
    for (int i = 0; i < num_polling_stations; i++) {
        pthread_cancel(mechanic_threads[i]);
        pthread_join(mechanic_threads[i], NULL);
    }

    for (PollingStation &station : polling_stations) {
        pthread_mutex_destroy(&station.mutex);
        pthread_cond_destroy(&station.next_voter_cond);
        pthread_cond_destroy(&station.mechanic_cond);
        pthread_cancel(display_votes_thread);
        pthread_join(display_votes_thread, NULL);
        std::cout << "Polling station " << station.id << " results:" << std::endl;
        std::cout << "Votes for Mary: " << station.votes_mary << std::endl;
        std::cout << "Votes for John: " << station.votes_john << std::endl;
        std::cout << "Votes for Anna: " << station.votes_anna << std::endl;

    }

    //close voters.log file
    voter_log.close();
    return 0;
}

