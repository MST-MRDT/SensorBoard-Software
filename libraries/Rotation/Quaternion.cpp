#include "Quaternion.h"

#define sampleFreq	512.0f			// sample frequency in Hz
#define twoKpDef	(2.0f * 0.5f)	// 2 * proportional gain
#define twoKiDef	(2.0f * 0.0f)	// 2 * integral gain

volatile float twoKp = twoKpDef;											// 2 * proportional gain (Kp)
volatile float twoKi = twoKiDef;											// 2 * integral gain (Ki)
volatile float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;					// quaternion of sensor frame relative to auxiliary frame
volatile float integralFBx = 0.0f,  integralFBy = 0.0f, integralFBz = 0.0f;	// integral error terms scaled by Ki

float invSqrt(float x) {
	float halfx = 0.5f * x;
	float y = x;
	long i = *(long*)&y;
	i = 0x5f3759df - (i>>1);
	y = *(float*)&i;
	y = y * (1.5f - (halfx * y * y));
	return y;
}

//Class provides sensor fusion allowing heading, pitch and roll to be extracted. This uses the Madgwick algorithm.
//The update method must be called periodically. The calculations take 1.6mS on the Pyboard.

float radians(float degrees)
{
  return ( degrees * 3.14159265359 ) / 180 ;
}

float degrees(float radians)
{
  return ( radians * 180 ) / 3.14159265359 ;
}

void Quaternion::init()
{
  magbias[0] = 0;
  magbias[1] = 0;
  magbias[2] = 0;
  q[0] = 1.0;
  q[1] = 0.0;
  q[2] = 0.0; 
  q[3] = 0.0;
  GyroMeasError = radians(40); // adjust this value to adjust between accuracy and speed
  beta = sqrt(3.0 / 4.0) * GyroMeasError;  // compute betas
  pitch = 0;
  heading = 0;
  headingOffset = 0;
  trueHeading = 0;
  roll = 0;
  deltat = .25;
  //in the form x,y,z  (0,1,2) 
  magmax[0] =  0.56;
  magmax[1] =  -0.0836379998922348;
  magmax[2] =  -0.7745999872684479;
  magmin[0] =  -0.10232999950647354;
  magmin[1] =  -0.7232999950647354;
  magmin[2] =  -1.29509999990463257;
  
  magbias[0] = (magmax[0] + magmin[0])/2;
  magbias[1] = (magmax[1] + magmin[1])/2;
  magbias[2] = (magmax[2] + magmin[2])/2;

  headingOffset = -76;
}

void Quaternion::updateMadgwick(float gyroscopeXYZ[], float accelerometerXYZ[], float magnetometerXYZ[])    // 3-tuples (x, y, z) for accel, gyro and mag data
{
        float mx = magnetometerXYZ[0] - magbias[0];
        float my = magnetometerXYZ[1] - magbias[1];
        float mz = magnetometerXYZ[2] - magbias[2];
        float ax = accelerometerXYZ[0];
        float ay = accelerometerXYZ[1];
        float az = accelerometerXYZ[2];
        float gx = radians(gyroscopeXYZ[0]);
        float gy = radians(gyroscopeXYZ[1]);
        float gz = radians(gyroscopeXYZ[2]);
        float q1 = q[0];
        float q2 = q[1];
        float q3 = q[2];
        float q4 = q[3];
        float norm;
        float hx, hy, _2bx, _2bz;
        float s1, s2, s3, s4;
        float qDot1, qDot2, qDot3, qDot4;

        // Auxiliary variables to avoid repeated arithmetic
        float _2q1mx;
        float _2q1my;
        float _2q1mz;
        float _2q2mx;
        float _4bx;
        float _4bz;
        float _2q1 = 2.0f * q1;
        float _2q2 = 2.0f * q2;
        float _2q3 = 2.0f * q3;
        float _2q4 = 2.0f * q4;
        float _2q1q3 = 2.0f * q1 * q3;
        float _2q3q4 = 2.0f * q3 * q4;
        float q1q1 = q1 * q1;
        float q1q2 = q1 * q2;
        float q1q3 = q1 * q3;
        float q1q4 = q1 * q4;
        float q2q2 = q2 * q2;
        float q2q3 = q2 * q3;
        float q2q4 = q2 * q4;
        float q3q3 = q3 * q3;
        float q3q4 = q3 * q4;
        float q4q4 = q4 * q4;

        // Normalise accelerometer measurement
        norm = sqrtf(ax * ax + ay * ay + az * az);
        if (norm == 0.0f) return; // handle NaN
        norm = 1.0f/norm;
        ax *= norm;
        ay *= norm;
        az *= norm;

        // Normalise magnetometer measurement
        norm = sqrtf(mx * mx + my * my + mz * mz);
        if (norm == 0.0f) return; // handle NaN
        norm = 1.0f/norm;
        mx *= norm;
        my *= norm;
        mz *= norm;

        // Reference direction of Earth's magnetic field
        _2q1mx = 2.0f * q1 * mx;
        _2q1my = 2.0f * q1 * my;
        _2q1mz = 2.0f * q1 * mz;
        _2q2mx = 2.0f * q2 * mx;
        hx = mx * q1q1 - _2q1my * q4 + _2q1mz * q3 + mx * q2q2 + _2q2 * my * q3 + _2q2 * mz * q4 - mx * q3q3 - mx * q4q4;
        hy = _2q1mx * q4 + my * q1q1 - _2q1mz * q2 + _2q2mx * q3 - my * q2q2 + my * q3q3 + _2q3 * mz * q4 - my * q4q4;
        _2bx = sqrtf(hx * hx + hy * hy);
        _2bz = -_2q1mx * q3 + _2q1my * q2 + mz * q1q1 + _2q2mx * q4 - mz * q2q2 + _2q3 * my * q4 - mz * q3q3 + mz * q4q4;
        _4bx = 2.0f * _2bx;
        _4bz = 2.0f * _2bz;

        // Gradient decent algorithm corrective step
        s1 = -_2q3 * (2.0f * q2q4 - _2q1q3 - ax) + _2q2 * (2.0f * q1q2 + _2q3q4 - ay) - _2bz * q3 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q4 + _2bz * q2) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q3 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
        s2 = _2q4 * (2.0f * q2q4 - _2q1q3 - ax) + _2q1 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q2 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + _2bz * q4 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q3 + _2bz * q1) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q4 - _4bz * q2) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
        s3 = -_2q1 * (2.0f * q2q4 - _2q1q3 - ax) + _2q4 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q3 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + (-_4bx * q3 - _2bz * q1) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q2 + _2bz * q4) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q1 - _4bz * q3) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
        s4 = _2q2 * (2.0f * q2q4 - _2q1q3 - ax) + _2q3 * (2.0f * q1q2 + _2q3q4 - ay) + (-_4bx * q4 + _2bz * q2) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q1 + _2bz * q3) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q2 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
        norm = sqrtf(s1 * s1 + s2 * s2 + s3 * s3 + s4 * s4);    // normalise step magnitude
        norm = 1.0f/norm;
        s1 *= norm;
        s2 *= norm;
        s3 *= norm;
        s4 *= norm;

        // Compute rate of change of quaternion
        qDot1 = 0.5f * (-q2 * gx - q3 * gy - q4 * gz) - beta * s1;
        qDot2 = 0.5f * (q1 * gx + q3 * gz - q4 * gy) - beta * s2;
        qDot3 = 0.5f * (q1 * gy - q2 * gz + q4 * gx) - beta * s3;
        qDot4 = 0.5f * (q1 * gz + q2 * gy - q3 * gx) - beta * s4;

        // Integrate to yield quaternion
        q1 += qDot1 * deltat;
        q2 += qDot2 * deltat;
        q3 += qDot3 * deltat;
        q4 += qDot4 * deltat;
        norm = sqrtf(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);    // normalise quaternion
        norm = 1.0f/norm;
        q[0] = q1 * norm;
        q[1] = q2 * norm;
        q[2] = q3 * norm;
        q[3] = q4 * norm;

        heading   = atan2f(2.0f * (q[1] * q[2] + q[0] * q[3]), q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3]);   
        roll = -asinf(2.0f * (q[1] * q[3] - q[0] * q[2]));
        pitch  = atan2f(2.0f * (q[0] * q[1] + q[2] * q[3]), q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3]);
        roll *= 180.0f / PI_VAL;
        heading   *= 180.0f / PI_VAL; 
        heading   -= 1.25; // Declination (update to include competition value
        pitch *= 180.0f / PI_VAL;
        pitch += pitchAdjust;
        trueHeading = fmod((((heading - 360) * -1) - 76), (float)360); 
}

void Quaternion::updateMahony(float gyroscopeXYZ[], float accelerometerXYZ[], float magnetometerXYZ[])    // 3-tuples (x, y, z) for accel, gyro and mag data
{
        float mx = magnetometerXYZ[0] - magbias[0];
        float my = magnetometerXYZ[1] - magbias[1];
        float mz = magnetometerXYZ[2] - magbias[2];
        float ax = accelerometerXYZ[0];
        float ay = accelerometerXYZ[1];
        float az = accelerometerXYZ[2];
        float gx = radians(gyroscopeXYZ[0]);
        float gy = radians(gyroscopeXYZ[1]);
        float gz = radians(gyroscopeXYZ[2]);
        float q1 = q[0], q2 = q[1], q3 = q[2], q4 = q[3];   // short name local variable for readability
        float norm;
        float hx, hy, bx, bz;
        float vx, vy, vz, wx, wy, wz;
        float ex, ey, ez;
        float pa, pb, pc;
        float Kp = 2.0f * 10.0f; // these are the free parameters in the Mahony filter and fusion scheme, Kp for proportional feedback, Ki for integral
        float Ki = 0.0f;
        float eInt[3] = {0.0f, 0.0f, 0.0f}; 

        // Auxiliary variables to avoid repeated arithmetic
        float q1q1 = q1 * q1;
        float q1q2 = q1 * q2;
        float q1q3 = q1 * q3;
        float q1q4 = q1 * q4;
        float q2q2 = q2 * q2;
        float q2q3 = q2 * q3;
        float q2q4 = q2 * q4;
        float q3q3 = q3 * q3;
        float q3q4 = q3 * q4;
        float q4q4 = q4 * q4;   

        // Normalise accelerometer measurement
        norm = sqrtf(ax * ax + ay * ay + az * az);
        if (norm == 0.0f) return; // handle NaN
        norm = 1.0f / norm;        // use reciprocal for division
        ax *= norm;
        ay *= norm;
        az *= norm;

        // Normalise magnetometer measurement
        norm = sqrtf(mx * mx + my * my + mz * mz);
        if (norm == 0.0f) return; // handle NaN
        norm = 1.0f / norm;        // use reciprocal for division
        mx *= norm;
        my *= norm;
        mz *= norm;

        // Reference direction of Earth's magnetic field
        hx = 2.0f * mx * (0.5f - q3q3 - q4q4) + 2.0f * my * (q2q3 - q1q4) + 2.0f * mz * (q2q4 + q1q3);
        hy = 2.0f * mx * (q2q3 + q1q4) + 2.0f * my * (0.5f - q2q2 - q4q4) + 2.0f * mz * (q3q4 - q1q2);
        bx = sqrtf((hx * hx) + (hy * hy));
        bz = 2.0f * mx * (q2q4 - q1q3) + 2.0f * my * (q3q4 + q1q2) + 2.0f * mz * (0.5f - q2q2 - q3q3);

        // Estimated direction of gravity and magnetic field
        vx = 2.0f * (q2q4 - q1q3);
        vy = 2.0f * (q1q2 + q3q4);
        vz = q1q1 - q2q2 - q3q3 + q4q4;
        wx = 2.0f * bx * (0.5f - q3q3 - q4q4) + 2.0f * bz * (q2q4 - q1q3);
        wy = 2.0f * bx * (q2q3 - q1q4) + 2.0f * bz * (q1q2 + q3q4);
        wz = 2.0f * bx * (q1q3 + q2q4) + 2.0f * bz * (0.5f - q2q2 - q3q3);  

        // Error is cross product between estimated direction and measured direction of gravity
        ex = (ay * vz - az * vy) + (my * wz - mz * wy);
        ey = (az * vx - ax * vz) + (mz * wx - mx * wz);
        ez = (ax * vy - ay * vx) + (mx * wy - my * wx);
        if (Ki > 0.0f)
        {
            eInt[0] += ex;      // accumulate integral error
            eInt[1] += ey;
            eInt[2] += ez;
        }
        else
        {
            eInt[0] = 0.0f;     // prevent integral wind up
            eInt[1] = 0.0f;
            eInt[2] = 0.0f;
        }

        // Apply feedback terms
        gx = gx + Kp * ex + Ki * eInt[0];
        gy = gy + Kp * ey + Ki * eInt[1];
        gz = gz + Kp * ez + Ki * eInt[2];

        // Integrate rate of change of quaternion
        pa = q2;
        pb = q3;
        pc = q4;
        q1 = q1 + (-q2 * gx - q3 * gy - q4 * gz) * (0.5f * deltat);
        q2 = pa + (q1 * gx + pb * gz - pc * gy) * (0.5f * deltat);
        q3 = pb + (q1 * gy - pa * gz + pc * gx) * (0.5f * deltat);
        q4 = pc + (q1 * gz + pa * gy - pb * gx) * (0.5f * deltat);

        // Normalise quaternion
        norm = sqrtf(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);
        norm = 1.0f / norm;
        q[0] = q1 * norm;
        q[1] = q2 * norm;
        q[2] = q3 * norm;
        q[3] = q4 * norm;
        heading   = atan2(2.0f * (q[1] * q[2] + q[0] * q[3]), q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3]);   
        roll = -asin(2.0f * (q[1] * q[3] - q[0] * q[2]));
        pitch  = atan2(2.0f * (q[0] * q[1] + q[2] * q[3]), q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3]);
        roll *= 180.0f / PI_VAL;
        heading   *= 180.0f / PI_VAL; 
        heading   -= 1.25; // Declination (update to include competition value
        pitch *= 180.0f / PI_VAL;
        pitch += pitchAdjust;
        trueHeading = fmod((((heading - 360) * -1) - 76), (float)360); 
}

float Quaternion::getHeading()
{
    return heading;
}

float Quaternion::getPitch()
{
    return pitch;
}

float Quaternion::getRoll()
{
    return roll;
}

float Quaternion::getTrueHeading()
{
    return trueHeading;
}

void Quaternion::MahonyAHRSupdate(float gyroscopeXYZ[], float accelerometerXYZ[], float magnetometerXYZ[]) {
	float mx =-(magnetometerXYZ[0] );
    float my = (magnetometerXYZ[1] );
    float mz = (magnetometerXYZ[2] );
    float ax = (accelerometerXYZ[0]            );
    float ay = (accelerometerXYZ[1]            );
    float az = (accelerometerXYZ[2]            );
    float gx = PI_VAL/180.0f*(radians(gyroscopeXYZ[0])       );
    float gy = PI_VAL/180.0f*(radians(gyroscopeXYZ[1])       );
    float gz = PI_VAL/180.0f*(radians(gyroscopeXYZ[2])       );
		
	float recipNorm;
    float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;  
	float hx, hy, bx, bz;
	float halfvx, halfvy, halfvz, halfwx, halfwy, halfwz;
	float halfex, halfey, halfez;
	float qa, qb, qc;

	// Use IMU algorithm if magnetometer measurement invalid (avoids NaN in magnetometer normalisation)
	if((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)) {
		return;
	}

	// Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
	if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

		// Normalise accelerometer measurement
		recipNorm = invSqrt(ax * ax + ay * ay + az * az);
		ax *= recipNorm;
		ay *= recipNorm;
		az *= recipNorm;     

		// Normalise magnetometer measurement
		recipNorm = invSqrt(mx * mx + my * my + mz * mz);
		mx *= recipNorm;
		my *= recipNorm;
		mz *= recipNorm;   

        // Auxiliary variables to avoid repeated arithmetic
        q0q0 = q0 * q0;
        q0q1 = q0 * q1;
        q0q2 = q0 * q2;
        q0q3 = q0 * q3;
        q1q1 = q1 * q1;
        q1q2 = q1 * q2;
        q1q3 = q1 * q3;
        q2q2 = q2 * q2;
        q2q3 = q2 * q3;
        q3q3 = q3 * q3;   

        // Reference direction of Earth's magnetic field
        hx = 2.0f * (mx * (0.5f - q2q2 - q3q3) + my * (q1q2 - q0q3) + mz * (q1q3 + q0q2));
        hy = 2.0f * (mx * (q1q2 + q0q3) + my * (0.5f - q1q1 - q3q3) + mz * (q2q3 - q0q1));
        bx = sqrt(hx * hx + hy * hy);
        bz = 2.0f * (mx * (q1q3 - q0q2) + my * (q2q3 + q0q1) + mz * (0.5f - q1q1 - q2q2));

		// Estimated direction of gravity and magnetic field
		halfvx = q1q3 - q0q2;
		halfvy = q0q1 + q2q3;
		halfvz = q0q0 - 0.5f + q3q3;
        halfwx = bx * (0.5f - q2q2 - q3q3) + bz * (q1q3 - q0q2);
        halfwy = bx * (q1q2 - q0q3) + bz * (q0q1 + q2q3);
        halfwz = bx * (q0q2 + q1q3) + bz * (0.5f - q1q1 - q2q2);  
	
		// Error is sum of cross product between estimated direction and measured direction of field vectors
		halfex = (ay * halfvz - az * halfvy) + (my * halfwz - mz * halfwy);
		halfey = (az * halfvx - ax * halfvz) + (mz * halfwx - mx * halfwz);
		halfez = (ax * halfvy - ay * halfvx) + (mx * halfwy - my * halfwx);

		// Compute and apply integral feedback if enabled
		if(twoKi > 0.0f) {
			integralFBx += twoKi * halfex * (1.0f / sampleFreq);	// integral error scaled by Ki
			integralFBy += twoKi * halfey * (1.0f / sampleFreq);
			integralFBz += twoKi * halfez * (1.0f / sampleFreq);
			gx += integralFBx;	// apply integral feedback
			gy += integralFBy;
			gz += integralFBz;
		}
		else {
			integralFBx = 0.0f;	// prevent integral windup
			integralFBy = 0.0f;
			integralFBz = 0.0f;
		}

		// Apply proportional feedback
		gx += twoKp * halfex;
		gy += twoKp * halfey;
		gz += twoKp * halfez;
	}
	
	// Integrate rate of change of quaternion
	gx *= (0.5f * (1.0f / sampleFreq));		// pre-multiply common factors
	gy *= (0.5f * (1.0f / sampleFreq));
	gz *= (0.5f * (1.0f / sampleFreq));
	qa = q0;
	qb = q1;
	qc = q2;
	q0 += (-qb * gx - qc * gy - q3 * gz);
	q1 += (qa * gx + qc * gz - q3 * gy);
	q2 += (qa * gy - qb * gz + q3 * gx);
	q3 += (qa * gz + qb * gy - qc * gx); 
	
	// Normalise quaternion
	recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
	q0 *= recipNorm;
	q1 *= recipNorm;
	q2 *= recipNorm;
	q3 *= recipNorm;
	
	heading   = atan2f(2.0f * (q1 * q2 + q0 * q3), q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3);   
    roll = -asinf(2.0f * (q1 * q3 - q0 * q2));
    pitch  = atan2f(2.0f * (q0 * q1 + q2 * q3), q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3);
    roll *= 180.0f / PI_VAL;
    heading   *= 180.0f / PI_VAL; 
    heading   -= 1.25; // Declination (update to include competition value
    pitch *= 180.0f / PI_VAL;
    pitch += pitchAdjust;
    trueHeading = fmod((((heading - 360) * -1) - 76), (float)360); 
}


void Quaternion::MadgwickQuaternionUpdate(float gyroscopeXYZ[], float accelerometerXYZ[], float magnetometerXYZ[]) {
	float mx =-(magnetometerXYZ[0] - magbias[0]);
    float my = (magnetometerXYZ[1] - magbias[1]);
    float mz = (magnetometerXYZ[2] - magbias[2]);
    float ax = (accelerometerXYZ[0]            );
    float ay = (accelerometerXYZ[1]            );
    float az = (accelerometerXYZ[2]            );
    float gx = PI_VAL/180.0f*(radians(gyroscopeXYZ[0])       );
    float gy = PI_VAL/180.0f*(radians(gyroscopeXYZ[1])       );
    float gz = PI_VAL/180.0f*(radians(gyroscopeXYZ[2])       );
	
    float q1 = q[0], q2 = q[1], q3 = q[2], q4 = q[3];   // short name local variable for readability
    float norm;
    float hx, hy, _2bx, _2bz;
    float s1, s2, s3, s4;
    float qDot1, qDot2, qDot3, qDot4;

    // Auxiliary variables to avoid repeated arithmetic
    float _2q1mx;
    float _2q1my;
    float _2q1mz;
    float _2q2mx;
    float _4bx;
    float _4bz;
    float _2q1 = 2.0f * q1;
    float _2q2 = 2.0f * q2;
    float _2q3 = 2.0f * q3;
    float _2q4 = 2.0f * q4;
    float _2q1q3 = 2.0f * q1 * q3;
    float _2q3q4 = 2.0f * q3 * q4;
    float q1q1 = q1 * q1;
    float q1q2 = q1 * q2;
    float q1q3 = q1 * q3;
    float q1q4 = q1 * q4;
    float q2q2 = q2 * q2;
    float q2q3 = q2 * q3;
    float q2q4 = q2 * q4;
    float q3q3 = q3 * q3;
    float q3q4 = q3 * q4;
    float q4q4 = q4 * q4;

    // Normalise accelerometer measurement
    norm = sqrt(ax * ax + ay * ay + az * az);
    if (norm == 0.0f) return; // handle NaN
    norm = 1.0f/norm;
    ax *= norm;
    ay *= norm;
    az *= norm;

    // Normalise magnetometer measurement
    norm = sqrt(mx * mx + my * my + mz * mz);
    if (norm == 0.0f) return; // handle NaN
    norm = 1.0f/norm;
    mx *= norm;
    my *= norm;
    mz *= norm;

    // Reference direction of Earth's magnetic field
    _2q1mx = 2.0f * q1 * mx;
    _2q1my = 2.0f * q1 * my;
    _2q1mz = 2.0f * q1 * mz;
    _2q2mx = 2.0f * q2 * mx;
    hx = mx * q1q1 - _2q1my * q4 + _2q1mz * q3 + mx * q2q2 + _2q2 * my * q3 + _2q2 * mz * q4 - mx * q3q3 - mx * q4q4;
    hy = _2q1mx * q4 + my * q1q1 - _2q1mz * q2 + _2q2mx * q3 - my * q2q2 + my * q3q3 + _2q3 * mz * q4 - my * q4q4;
    _2bx = sqrt(hx * hx + hy * hy);
    _2bz = -_2q1mx * q3 + _2q1my * q2 + mz * q1q1 + _2q2mx * q4 - mz * q2q2 + _2q3 * my * q4 - mz * q3q3 + mz * q4q4;
    _4bx = 2.0f * _2bx;
    _4bz = 2.0f * _2bz;

    // Gradient decent algorithm corrective step
    s1 = -_2q3 * (2.0f * q2q4 - _2q1q3 - ax) + _2q2 * (2.0f * q1q2 + _2q3q4 - ay) - _2bz * q3 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q4 + _2bz * q2) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q3 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
    s2 = _2q4 * (2.0f * q2q4 - _2q1q3 - ax) + _2q1 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q2 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + _2bz * q4 * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q3 + _2bz * q1) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q4 - _4bz * q2) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
    s3 = -_2q1 * (2.0f * q2q4 - _2q1q3 - ax) + _2q4 * (2.0f * q1q2 + _2q3q4 - ay) - 4.0f * q3 * (1.0f - 2.0f * q2q2 - 2.0f * q3q3 - az) + (-_4bx * q3 - _2bz * q1) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (_2bx * q2 + _2bz * q4) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + (_2bx * q1 - _4bz * q3) * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
    s4 = _2q2 * (2.0f * q2q4 - _2q1q3 - ax) + _2q3 * (2.0f * q1q2 + _2q3q4 - ay) + (-_4bx * q4 + _2bz * q2) * (_2bx * (0.5f - q3q3 - q4q4) + _2bz * (q2q4 - q1q3) - mx) + (-_2bx * q1 + _2bz * q3) * (_2bx * (q2q3 - q1q4) + _2bz * (q1q2 + q3q4) - my) + _2bx * q2 * (_2bx * (q1q3 + q2q4) + _2bz * (0.5f - q2q2 - q3q3) - mz);
    norm = sqrt(s1 * s1 + s2 * s2 + s3 * s3 + s4 * s4);    // normalise step magnitude
    norm = 1.0f/norm;
    s1 *= norm;
    s2 *= norm;
    s3 *= norm;
    s4 *= norm;

    // Compute rate of change of quaternion
    qDot1 = 0.5f * (-q2 * gx - q3 * gy - q4 * gz) - beta * s1;
    qDot2 = 0.5f * (q1 * gx + q3 * gz - q4 * gy) - beta * s2;
    qDot3 = 0.5f * (q1 * gy - q2 * gz + q4 * gx) - beta * s3;
    qDot4 = 0.5f * (q1 * gz + q2 * gy - q3 * gx) - beta * s4;

    // Integrate to yield quaternion
    q1 += qDot1 * deltat;
    q2 += qDot2 * deltat;
    q3 += qDot3 * deltat;
    q4 += qDot4 * deltat;
    norm = sqrt(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);    // normalise quaternion
    norm = 1.0f/norm;
    q[0] = q1 * norm;
    q[1] = q2 * norm;
    q[2] = q3 * norm;
    q[3] = q4 * norm;
	
	heading   = atan2f(2.0f * (q[1] * q[2] + q[0] * q[3]), q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3]);   
    roll = -asinf(2.0f * (q[1] * q[3] - q[0] * q[2]));
    pitch  = atan2f(2.0f * (q[0] * q[1] + q[2] * q[3]), q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3]);
    roll *= 180.0f / PI_VAL;
    heading   *= 180.0f / PI_VAL; 
    heading   -= 1.25; // Declination (update to include competition value
    pitch *= 180.0f / PI_VAL;
    pitch += pitchAdjust;
    trueHeading = fmod((((heading - 360) * -1) - 76), (float)360); \
}