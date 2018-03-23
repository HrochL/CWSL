// Alex Ranaldi  W2AXR   alexranaldi@gmail.com

// LICENSE: GNU General Public License v3.0
// THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.

#include <vector>
#include <algorithm>

// Auto computes a factor that can be used to scale audio data.
//  This is essentially a volume knob that turns itself.
template <class T>
class AutoScaleAF{
    
    public:
            
        // headroomdB specifies the ideal headroom in dB to maintain
        // maxVal specifies the max. e.g., a value that represents a clipped value
        AutoScaleAF(const T headroomdB, const T maxVal) : 
            mScaleFactor(0),
            mHeadroomdB(headroomdB),
            mSinceDecrease(0) {
                
            mMaxdB = v_to_db(maxVal);
            mThreshdB = mMaxdB - headroomdB;
            mThreshV = db_to_v(mThreshdB);
            
            mAmountDecrease = 1/db_to_v(3);
            mAmountIncrease = db_to_v(1);
        }
        
        virtual ~AutoScaleAF(){
        }
    
        T getScaleFactor(const std::vector<T> v){
            computeScaleFactor(v);
            return mScaleFactor;
        }
        
    private:
    
        // accepts samples and recomputes the scale factor
        void computeScaleFactor(const std::vector<T> v){
            // If no scale factor has been determined yet
            if (0 == mScaleFactor){
                // initial scale factor
                const auto maxIt = max_element(v.begin(), v.end());
                const T maxVal = *maxIt;
                mScaleFactor = mThreshV / maxVal;
            }
            else {
                const size_t numGT = num_greater_than_abs(v, mThreshV / mScaleFactor);
                const T percentGT = static_cast<T>(numGT) / static_cast<T>(v.size());
                mSinceDecrease++;
                if (percentGT >= 0.025){
                    mScaleFactor *= mAmountDecrease;
                    mSinceDecrease = 0;
                }
                else if ((percentGT <= 0.65) && (mSinceDecrease > 500)){
                    mScaleFactor *= mAmountIncrease;
                }
            }
        }
    
        T v_to_db(const T v_in) const{
            return 20*std::log10(v_in);
        }
        
        T db_to_v(const T db_in) const{
            return std::pow(10, db_in / 20);
        }

        size_t num_greater_than_abs(const std::vector<T> v, const T test) const{
            size_t count = 0;
            for (size_t k = 0; k < v.size(); ++k){
                if ( (v[k] > test) || (v[k] < -test) ) {
                    count++;
                }
            }
            return count;
        }
       
        T mScaleFactor;
        T mHeadroomdB;
        T mMaxdB;
        T mThreshV;
        T mThreshdB;
        T mAmountDecrease;
        T mAmountIncrease;
        size_t mSinceDecrease;
        
 };