#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>
#include <random>
#include "simple_random.h"
#include "timer.h"
#include "fvector.h"
#include "tools.h"
#include "examples.h"
#include "global_macros.h"

using namespace std;

void permute(simple_random &rand, int *d, int n) {
  for(int i = n - 1; i > 0; i--) {
    int rand_index = rand.rand_int(i); // pick something [0,i]
    int temp       = d[i];
    d[i]           = d[rand_index];
    d[rand_index]  = temp;    
  }
  VERBOSE_ONLY(for(int i = 0; i < n; i++){cout << i << " -> " << d[i] << endl;})
}

int* init_permutation(int nSize) {
  int *ret = new int[nSize];
  for(int i = nSize - 1; i >= 0; i--) { ret[i] = i; }
  return ret;
}

struct permute_thread_info {
  simple_random &rand; 
  int *r, n;
  permute_thread_info(simple_random &_rand, int* _r, int _n) : rand(_rand), r(_r), n(_n) { } 
};

void* permute_thread( void* p ) {
  struct permute_thread_info *pti = (struct permute_thread_info*) p;
  permute(pti->rand, pti->r, pti->n);
  return NULL;
}

struct gradient_thread_info {
    int id, nWorkers, nTrain;
    FVector *X, *Y;
    Example* ex;
    int* perm;
    int* sample;
    double cur_learning_rate, lambda;
    gradient_thread_info(int _id, int _nWorkers, int _nTrain, 
            FVector* _X, FVector* _Y, Example* _ex, int* _perm, int* _sample,
            double _cur_learning_rate, double _lambda){
        id = _id; nWorkers = _nWorkers; nTrain = _nTrain;
        X = _X; Y = _Y; ex = _ex; perm = _perm; sample = _sample;
        cur_learning_rate = _cur_learning_rate; lambda = _lambda;
    }
};

void* gradient_thread(void* params) {
    struct gradient_thread_info* gti = (struct gradient_thread_info*)(params);
    int id = gti->id;
    int nWorkers = gti->nWorkers;
    int* perm = gti->perm;
    int* sample = gti->sample;
    FVector *X = gti->X;
    FVector *Y = gti->Y;
    struct Example* examples = gti->ex;
    int nTrain = gti->nTrain;
    double cur_learning_rate = gti->cur_learning_rate;
    double lambda = gti->lambda;

    // Calculate offset for examples
    int start_offset = id * (nTrain / nWorkers);
    int end_offset   = min(nTrain, (id + 1) * (nTrain / nWorkers));
    DEBUG_ONLY(cout << "start=" << start_offset << " -- " << end_offset << " " << nExamples << " nWorkers=" << nWorkers << " id=" << id << endl;)

    for (int i = start_offset; i < end_offset; i++){
        // Read example
        int pi = sample[perm[i]];
        int row_index = examples[pi].row;
        int col_index = examples[pi].col;
        double rating = examples[pi].rating;
        double predict = FVector::dot(X[row_index], Y[col_index]);

        // Apply Gradient for Absolute Loss
        if (rating - predict > 0){
            FVector gradXi = Y[col_index];
            gradXi.scale(-1);
            gradXi.scale_and_add(X[row_index], lambda); 
            X[row_index].scale_and_add(gradXi, -cur_learning_rate);
           
            FVector gradYj = X[row_index];
            gradYj.scale(-1);
            gradYj.scale_and_add(Y[col_index], lambda);
            Y[col_index].scale_and_add(gradYj, -cur_learning_rate);
        } else if (rating - predict < 0) {
            FVector gradXi = Y[col_index];
            gradXi.scale_and_add(X[row_index], lambda); 
            X[row_index].scale_and_add(gradXi, -cur_learning_rate);
           
            FVector gradYj = X[row_index];
            gradYj.scale_and_add(Y[col_index], lambda);
            Y[col_index].scale_and_add(gradYj, -cur_learning_rate);
        }

        // Apply Gradient for Sigmoid Loss
//        double den = pow(1 + exp(predict * rating), 2); 
//        
//        FVector gradXi = Y[col_index]; // need to multiply Yj
//        gradXi.scale(-exp(rating * predict) * rating / den);
//        gradXi.scale_and_add(X[row_index], lambda); 
//
//        X[row_index].scale_and_add(gradXi, -cur_learning_rate);
//
//        FVector gradYj = X[row_index]; // need to multiply by Xi
//        gradYj.scale(-exp(rating * predict) * rating / den);
//        gradYj.scale_and_add(Y[col_index], lambda);
//
//        Y[col_index].scale_and_add(gradYj, -cur_learning_rate);

        // Apply Gradient for Square Loss
//        FVector gradXi = Y[col_index];
//        gradXi.scale(2 * (predict - rating));
//        gradXi.scale_and_add(X[row_index], lambda);
//
//        X[row_index].scale_and_add(gradXi, -cur_learning_rate);
//
//        FVector gradYj = X[row_index];
//        gradYj.scale(2 * (predict - rating));
//        gradYj.scale_and_add(Y[col_index], lambda);
//
//        Y[col_index].scale_and_add(gradYj, -cur_learning_rate);

        // Apply Gradient for Square-hinge Loss
//        if (rating * predict < 1){
//            FVector gradXi = Y[col_index];
//            gradXi.scale(2 * (rating * predict - 1) * rating);
//            gradXi.scale_and_add(X[row_index], lambda);
//            X[row_index].scale_and_add(gradXi, -cur_learning_rate);
//            
//            FVector gradYj = X[row_index];
//            gradYj.scale(2 * (rating * predict - 1) * rating);
//            gradYj.scale_and_add(Y[col_index], lambda);
//            Y[col_index].scale_and_add(gradYj, -cur_learning_rate);
//        } 
//        else{
//            X[row_index].scale(1 - cur_learning_rate * lambda);
//            Y[col_index].scale(1 - cur_learning_rate * lambda);
//        }
    }
    return NULL;
}

int main(int argv, char *argc[]){
    const char* inputFile = "./data/Epinions/my_epinions.txt";
    //const char* inputFile = "./data/Slashdot/my_slashdot.txt";
    int nRows, nCols, nExamples;
    Example* examples = load_examples(inputFile, nRows, nCols, nExamples);
    
    bool printM = false;
    bool printXY = false;
    std::cout << "nRows: " << nRows  << " nCols: " << nCols << " nExamples: " << nExamples << std::endl;
    printf("Rank: %d \n", FVector::_default_n);
    //for (int i = 0; i < nExamples; i++) std::cout << examples[i].row << " " << examples[i].col << " " << examples[i].rating << std::endl;
    

    FVector* X = new FVector[nRows];
    FVector* Y = new FVector[nCols];
    
    int* sample = init_permutation(nExamples);
    simple_random rd;
    permute(rd, sample, nExamples);

    // Variables Update
    int maxEpoch = 100;
    double learning_rate = 0.022;
    double cur_learning_rate = learning_rate;
    int nWorkers = 1;
    double sample_rate = 0.9;
    double lambda = 0.1;
    int nTrain = int(nExamples * sample_rate);
    int nTest = nExamples - nTrain;
    std::cout << "Lambda: " << lambda << " nWorkers: " << nWorkers << std::endl;
    
    std::vector<double> Acc;
    std::vector<double> Rmse;
    std::vector<int> Epoch;
    std::vector<double> Time;
    double maxAcc = 0;
     
    std::cout << "Start Training ... " << std::endl;
    timer train_time(true);
    int* shared_perm = init_permutation(nTrain);
    //printVec(shared_perm, nTrain); 
    simple_random rd1;
    struct permute_thread_info* pti = new permute_thread_info(rd1, shared_perm, nTrain);

    gradient_thread_info* wtis[nWorkers];
    for (int i = 0; i < nWorkers; i++){
        wtis[i] = new gradient_thread_info(i, nWorkers, nTrain, X, Y, examples, 
                                            shared_perm, sample, cur_learning_rate, lambda);
    }

    for (int epoch = 0; epoch < maxEpoch; epoch++){
        pthread_t shuffler_t;
        cur_learning_rate = learning_rate / pow(1 + epoch, 0.1);
        int ret = pthread_create( &shuffler_t, NULL, permute_thread, (void*)pti);
        if(ret != 0) { 
            cout << "Error in pthread_create: " << ret << endl;
            exit(-1); 
        }

        pthread_t workers[nWorkers];
        for(int i = 0; i < nWorkers; i++) {
            wtis[i]->perm = pti->r;
            wtis[i]->cur_learning_rate = cur_learning_rate;
            int ret = pthread_create( &workers[i], NULL, gradient_thread, (void*) wtis[i]);
            if(ret != 0) { 
                cout << "Error in pthread_create: " << ret << endl;
                exit(-1); 
            }
        }
        // Thread Join
        for(int i = 0; i < nWorkers; i++) {
            pthread_join(workers[i], NULL);
        }
        pthread_join(shuffler_t, NULL);
        
        // Test Error and Accuracy 
        int trueNum = 0;
        long double error = 0;
        for (int i = nTrain + 1; i < nExamples; i++){
            int pi = sample[i]; 
            int row_index = examples[pi].row;
            int col_index = examples[pi].col;
            double rating = examples[pi].rating;

            double predict = FVector::dot(X[row_index], Y[col_index]);
            if ( sign(predict) == sign(rating) ) trueNum++;
            error += pow(predict - rating, 2);
        }
        Acc.push_back(double(trueNum) / nTest);
        Rmse.push_back(sqrt(error / nTest));
        Epoch.push_back(epoch + 1);
        train_time.stop();
        Time.push_back(train_time.elapsed());
        maxAcc = max(maxAcc, Acc.back());
        printf("Epoch: %d   Accuracy: %.4f  RMSE: %.4f  Spend Time: %.2f s \n", Epoch.back(), Acc.back(), Rmse.back(), Time.back() ); 
    }
    //  OutPut File
    std::ofstream out("./Output/out.txt");  // Epoch, Acc, RMSE, Time
    if (out.is_open()){
        printf("write to output file\n");
        for (int i = 0; i < Epoch.size(); i++){
            out << Epoch[i] << " " << Acc[i] << " " << Rmse[i] << " " << Time[i] << "\n";
        }
        out.close();
    }

    if (printXY){
        std::ofstream out_x("./Output/X.txt"); // matrix X
        if (out_x.is_open()){
            printf("write to output X file \n");
            for (int i = 0; i < nRows; i++){
                for (int j = 0; j < FVector::_default_n; j++){
                    out_x << X[i].get(j) << " ";
                }
                out_x << "\n";
            }
            out_x.close();
        }
    
        std::ofstream out_y("./Output/Y.txt"); // matrix Y
        if (out_y.is_open()){
            printf("write to output Y file \n");
            for (int i = 0; i < nCols; i++){
                for (int j = 0; j < FVector::_default_n; j++){
                    out_y << Y[i].get(j) << " ";
                }
                out_y << "\n";
            }
            out_y.close();
        }
    }
    if (printM){
        long long p_num = 0; // positive number of rating
        long long n_num = 0; // negative number of rating
        long long zero_num = 0; // number of zero
        long double p_sum = 0;
        long double n_sum = 0;
        long double p_square = 0;
        long double n_square = 0;
        std::ofstream out_m("./Output/M.txt"); // Final matrix M
        if (out_m.is_open()){
            printf("Write to output Matrix file\n");
            for (int i = 0; i < nRows; i++){
                for (int j = 0; j < nCols; j++){
                    double predict = FVector::dot(X[i], Y[j]);
                    if (predict > 0){  // positive rating
                        p_sum += predict;
                        p_square += (predict - 4.3417) * (predict - 4.3417);
                        p_num++;
                    }
                    else if (predict < 0){ // negative rating
                        n_sum += predict;
                        n_num++;
                        n_square += (predict + 0.5286) * (predict + 0.5286);
                    }
                    else zero_num++;  // zero_number     
                    if ((i + j) % 10000 == 0){ // shrink to 1w times
                        out_m << predict << " ";
                    }
                }
                out_m << "\n";
            }
            out_m.close();
        }
        printf("p_num: %lld, p_sum: %.4Lf, n_num: %lld, n_sum: %.4Lf, zero_num: %lld \n", p_num, p_sum, n_num, n_sum, zero_num);
        printf("p_square: %.4Lf, n_square: %.4Lf, p_delta: %.4Lf, n_delta: %.4Lf \n", p_square, n_square, p_square / (1 + p_num), n_square / (1 + n_num));
        printf("the positive average rating is %.4Lf, the negative average rating is %.4Lf \n", p_sum / (1 + p_num), n_sum / (1 + n_num) );
    }
    printf("Final max Accuracy is: %.4f \n", maxAcc);
    for (int i = 0; i < nWorkers; i++) delete wtis[i];
    delete shared_perm;
    delete pti;
    delete[] X;
    delete[] Y;
    return 0;
}
