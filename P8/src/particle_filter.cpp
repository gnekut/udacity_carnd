/*
GN 2017-10-23: Thoughts of possible errors:
1) Yaw angle, theta being (+/-) w.t.r. x-axis coordinates.
2) 

 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>
#include <random>

#include "helper_functions.h"
#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	/*
	Purpose: Initialize particle filter by creating distribution of particles surrounding the intitial GPS coordinate measurements
	Input(s): [x: GPS X-position],[y: GPS Y-position], [theta: vehicle heading]
	Output(s): N/A - Object assignments; [particles_: x, y, theta, weight, id for each newly created particle], [weights_: Collection of all particle weights in a single vector]
	*/

	num_particles_ = 50; // Initialize particle count

	// Noise Generators
	default_random_engine gen; // Generate uniform dist. numbers
	normal_distribution<double> dist_x(x, std[0]); // X-Gaussian Object
	normal_distribution<double> dist_y(y, std[1]); // Y-Gaussian Object
	normal_distribution<double> dist_theta(theta, std[2]); // Theta-Gaussian Object

	// Initialize Particles
	for (int i=0; i<num_particles_; i++) {
		Particle particle;
		particle.id = i;
		particle.x = dist_x(gen);
		particle.y = dist_y(gen);
		particle.theta = dist_theta(gen);
		particle.weight = 1.0;

		particles_.push_back(particle);
		weights_.push_back(particle.weight);
	}

	is_initialized_ = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	/*
	Purpose: Update all particles position based upon the movement of the vehicle.
	Input(s): [delta_t: ellapsed time between predition steps (s)], [std_pos: x/y/theta noise parameters], [velocity: speed of vehicle (m/s)], [yaw_rate: Rate of turning angle change (rad/s)]
	Output(s): N/A - Object assignments: [particles_: translated particle positions and heading]
	*/

	default_random_engine gen; // Generate uniform dist. numbers
	normal_distribution<double> dist_x(0, std_pos[0]); // X-Gaussian Object generator
	normal_distribution<double> dist_y(0, std_pos[1]); // Y-Gaussian Object generator
	normal_distribution<double> dist_theta(0, std_pos[2]); // Theta-Gaussian Object generator

	// Predict Particles Position
	for (int p=0; p<num_particles_; p++) {

		// Zero-yaw rate separation
		if (fabs(yaw_rate) > 0.0001) {
			particles_[p].x = particles_[p].x + (velocity/yaw_rate) * (sin(particles_[p].theta + yaw_rate * delta_t) - sin(particles_[p].theta));
			particles_[p].y = particles_[p].y + (velocity/yaw_rate) * (cos(particles_[p].theta) - cos(particles_[p].theta + yaw_rate * delta_t));
			particles_[p].theta = particles_[p].theta + (yaw_rate * delta_t);
		} else {
			particles_[p].x =  particles_[p].x + (velocity * cos(particles_[p].theta)* delta_t);
			particles_[p].y =  particles_[p].y + (velocity * sin(particles_[p].theta)* delta_t);
			particles_[p].theta = particles_[p].theta;
		}

		// Generate new particle informations w/ noise
		particles_[p].x += dist_x(gen);
		particles_[p].y += dist_y(gen);
		particles_[p].theta += dist_theta(gen);
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], std::vector<LandmarkObs> observations, Map map_landmarks) {
	/*
	Purpose: Particle weight updates based upon vehicle landmark observations and landmarks map location. All observsations are mapped from the relative position of the car, treated as (0,0), to the global coordinate system of each particle (i.e., potential car position).
	Input(s): [sensor_range: maximum distance sensor can reliably observe landmarks (m)], [std_landmark: sensor measurement noise], [observations: Relative x/y coordinates of potential landmarks with respect to the vehicle (m)], [map_landmarks: Collection of landmarks in global coordinate system]
	Output(s): N/A - Objects assignments: [particles_: Associated landmark per particle and weight assignment based upon euclidian distance], [weights_: Collection of all new particle weights, used in resampling step.]
	*/

	int ob_size = observations.size();
	
	// Pre-compute variables
	double gauss_norm = 1.0 / (2.0 * M_PI * std_landmark[0] * std_landmark[1]); // Gaussian normalizer
	double x_denom = (2.0 * std_landmark[0] * std_landmark[0]); // Gaussian X-demoninator
	double y_denom = (2.0 * std_landmark[1] * std_landmark[1]); // Gaussian Y-demoninator


	// ========== PARTICLE ITERATION ==========
	for (int p=0; p<num_particles_; p++) {

		// Reset particles associations and weight
		particles_[p].associations.clear();
		particles_[p].sense_x.clear();
		particles_[p].sense_y.clear();
		particles_[p].weight = 1.0;


		// =========== LANDMARK EXTRACTION ========== 
		// Remap Map struct to LandmarkObs struct and ensure landmark is within sensor range for particle
		std::vector<LandmarkObs> landmarks;
		for (int i=0; i<map_landmarks.landmark_list.size(); i++) {
			LandmarkObs lm;
			lm.id = map_landmarks.landmark_list[i].id_i;
			lm.x = map_landmarks.landmark_list[i].x_f;
			lm.y = map_landmarks.landmark_list[i].y_f;
			if (dist(lm.x, lm.y, particles_[p].x, particles_[p].y) < sensor_range) {
				landmarks.push_back(lm);
			}
		}


		// ============= COORDINATE TRANSFORMATION ==========
		// Transform local car observations to global reference frame (partible position + observation)
		std::vector<LandmarkObs> observations_p;
		for (int i=0; i<ob_size; i++) {
			LandmarkObs obs;
			obs.x = particles_[p].x + (observations[i].x * cos(particles_[p].theta)) - (observations[i].y * sin(particles_[p].theta));
			obs.y = particles_[p].y + (observations[i].x * sin(particles_[p].theta)) + (observations[i].y * cos(particles_[p].theta));
			observations_p.push_back(obs);
		}


		


		// ========== LANDMARK-OBSERVATION ASSOCIATION ==========
		// Return (observations_p) for only those observations with an associated landmark
		for (int j=0; j<ob_size; j++) {
			double least_dist = dist(landmarks[0].x, landmarks[0].y, observations_p[j].x, observations_p[j].y); // Init. distance
			observations_p[j].id = landmarks[0].id; // Init. closest ID
			for (int i=0; i<landmarks.size(); i++) {
				double obs_dist = dist(landmarks[i].x, landmarks[i].y, observations_p[j].x, observations_p[j].y);
				if (least_dist > obs_dist) {
					observations_p[j].id = landmarks[i].id;
					least_dist = obs_dist;
				}
			}
			particles_[p].associations.push_back(observations_p[j].id);
			particles_[p].sense_x.push_back(observations_p[j].x);
			particles_[p].sense_y.push_back(observations_p[j].y);
		}


		// ========== WEIGHT CALCULATIONS ============
		int assoc_cnt = particles_[p].associations.size();
		
		if (assoc_cnt == 0) {
			particles_[p].weight = 0.0;
		} else {
			for (int i=0; i<assoc_cnt; i++) {
				int obs_lm_id = particles_[p].associations[i];
				double obs_x = particles_[p].sense_x[i];
				double obs_y = particles_[p].sense_y[i];
				
				// Iterate over all landmarks to find associated.
				for (int l=0; l<landmarks.size(); l++) {
					LandmarkObs lm = landmarks[l];
					if (lm.id == obs_lm_id) {
						double x_num = pow((obs_x-lm.x), 2.0);
						double y_num = pow((obs_y-lm.y), 2.0);
						particles_[p].weight *= (gauss_norm) * exp(-1.0*((x_num/x_denom)+(y_num/y_denom)));
						//cout << "  Weight[" << p << ", " << l << "]:  " << particles_[p].weight << "\n";
					}
				}
			}
		}
		// GN 2017-10-26: Very clearly missed this step in prior commit. Not changing the weights vector for each particle would result in the resampling using the same weights on each iteration, completely defeating the purpose of the particle filter update steps.
		weights_[p] = particles_[p].weight;
	}
	// _____ end FOR: particles_[p] _______ 
}




void ParticleFilter::resample() {
	/*
	Purpose: Resample particles based upon weight assignment. The more weight a given particle has, the closer association it had with various landmark/observations, and therefore the more it will be sampled and used in further steps.
	Input(s): N/A (global objects)
	Output(s): N/A - Object assignemnts: [particles_: collection of new particles]
	*/

    default_random_engine gen;
    std::discrete_distribution<int> weight_dist(weights_.begin(), weights_.end());

    std::vector<Particle> particles_new;
    for (int p=0; p<num_particles_; p++) {
        int p_i = weight_dist(gen);
        particles_new.push_back(particles_[p_i]);

        /* GN 2017-10-25: Fresh pair of eyes caught mistake that [p_i] will assign the same particle to the same index multiple times if it is resamples. To rectify, used the simple push_back() method to create an entirely new vector.
        particles_new[p_i] = particles_[p_i];
        */

    }
    particles_ = particles_new;

}


string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
