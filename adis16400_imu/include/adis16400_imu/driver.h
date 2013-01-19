#ifndef DRIVER_H
#define DRIVER_H

#include <cmath>
#include <fstream>

#include <sensor_msgs/Imu.h>
#include <geometry_msgs/Vector3Stamped.h>


namespace adis16400_imu {
    static uint16_t getu16(char* i) { return reinterpret_cast<const boost::uint16_t &>(*i); }
    static int16_t get16(char* i) { return reinterpret_cast<const boost::int16_t &>(*i); }
    static int64_t get64(char* i) { return reinterpret_cast<const boost::int64_t &>(*i); }
    
    class Device {
        private:
            const std::string port;
            std::ifstream is;
            
            void open() {
                while(true) {
                    try {
                        is.exceptions(std::ifstream::goodbit);
                        is.close();
                        is.clear();
                        is.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);
                        is.open(port.c_str());
                        return;
                    } catch(const std::exception &exc) {
                        ROS_ERROR("error on open(%s): %s; reopening after delay", port.c_str(), exc.what());
                        boost::this_thread::sleep(boost::posix_time::seconds(1));
                    }
                }
            }
            
        public:
            Device(const std::string port): port(port) {
                is.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);
            }
            
            std::pair<sensor_msgs::Imu, geometry_msgs::Vector3Stamped> read(const std::string frame_id) {
                char data[32];
                while(true) {
                    try {
                        is.read(data, 32);
                        break;
                    } catch(const std::exception &exc) {
                        ROS_ERROR("error on read: %s; reopening", exc.what());
                        open();
                    }
                }
                
                sensor_msgs::Imu result;
                result.header.frame_id = frame_id;
                result.header.stamp.fromNSec(get64(data + 24));
                
                result.orientation_covariance[0] = -1; // indicate no orientation data
                
                static const double GYRO_CONVERSION = 0.05 * (2*M_PI/360); // convert to deg/s and then to rad/s
                result.angular_velocity.x = get16(data + 4 + 2*0) * GYRO_CONVERSION;
                result.angular_velocity.y = get16(data + 4 + 2*1) * GYRO_CONVERSION;
                result.angular_velocity.z = get16(data + 4 + 2*2) * GYRO_CONVERSION;
                result.angular_velocity_covariance[0] = result.angular_velocity_covariance[4] = result.angular_velocity_covariance[8] =
                    pow(0.9 * (2*M_PI/360), 2); // 0.9 deg/sec rms converted to rad/sec and then squared
                
                
                static const double ACC_CONVERSION = 3.33e-3 * 9.80665; // convert to g's and then to m/s^2
                result.linear_acceleration.x = -get16(data + 10 + 2*0) * ACC_CONVERSION;
                result.linear_acceleration.y = -get16(data + 10 + 2*1) * ACC_CONVERSION;
                result.linear_acceleration.z = -get16(data + 10 + 2*2) * ACC_CONVERSION;
                result.linear_acceleration_covariance[0] = result.linear_acceleration_covariance[4] = result.linear_acceleration_covariance[8] =
                    pow(9e-3 * 9.80665, 2); // 9 mg rms converted to m/s^2 and then squared
                
                
                geometry_msgs::Vector3Stamped mag_result;
                mag_result.header.frame_id = frame_id;
                mag_result.header.stamp.fromNSec(get64(data + 24));
                
                static const double MAG_CONVERSION = 0.5e-3 * 0.0001; // convert to gauss and then to tesla
                mag_result.vector.x = get16(data + 16 + 2*0) * MAG_CONVERSION;
                mag_result.vector.y = get16(data + 16 + 2*1) * MAG_CONVERSION;
                mag_result.vector.z = get16(data + 16 + 2*2) * MAG_CONVERSION;
                
                
                getu16(data + 0); // flags unused
                getu16(data + 2) * 2.418e-3; // supply voltage unused
                get16(data + 22) * 0.14 + 25; // temperature unused
                
                return make_pair(result, mag_result);
            }
    };
    
}

#endif
