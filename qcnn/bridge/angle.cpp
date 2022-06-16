#include "qfactory.hpp"
#include "qneuron.hpp"
#include <iostream>
#include <cmath>
#include <climits>
#include <omp.h>

#define CORES 32

using namespace Qrack;

float get_eigenvalue(int state) {
    int num_zeroes = 0;
    bool count = false;
    for(int i = CHAR_BIT * sizeof state - 1; i >= 0; i--)
    {
        if (count && (state & (1 << i)) == 0) {
            num_zeroes++;
        } else if (!count && (state & (1 << i)) != 0) {
            count = true;
        }
    }
    return num_zeroes % 2 ? -1.0 : 1.0;
}

real1 calc_expectation(const real1 *probabilities, int size, const real1 *eigenvalues) {
    real1 expectation = 0;
    for (int i = 0; i < size; i++) {
        expectation += eigenvalues[i] * probabilities[i];
    }

    return expectation;
}

real1 run_circuit(
    int input_size,
    const real1 *input,
    const real1 *thetas,
    const real1 *eigenvalues,
    int n_feature_qubits,
    int n_qubits,
    int feature_sv_size,
    int sv_size
) {
    QInterfacePtr q_reg = CreateQuantumInterface(QINTERFACE_OPTIMAL, n_qubits, 0);

    // Build initial state
    auto *extended_input = new real1[feature_sv_size] {};
    extend_input(extended_input, input, input_size);

    auto* input_state = new complex[sv_size];
    for (int i = 0; i < feature_sv_size; i++) {
         input_state[i] += extended_input[i];
    }

    // Set initial state
    q_reg->SetQuantumState(input_state);
    q_reg->UpdateRunningNorm();
    q_reg->NormalizeState();

    // Apply Hadamard to target qubit
    bitLenInt last_qubit = n_qubits - 1;
    q_reg->H(last_qubit);

    // Add parameters
    const real1 *first_theta = thetas;
    for (bitLenInt i = 0; i < last_qubit; i++) {
        bitLenInt controls[1] { i };
        q_reg->CU(controls, 1, last_qubit, first_theta[0], first_theta[1], first_theta[2]);
        first_theta += 3;
    }

    // Apply QFT-1 to extract relative phase
    q_reg->IQFT(0, n_feature_qubits);

    auto probabilities = new real1[feature_sv_size];
    bitCapInt mask = ((1 << n_feature_qubits) - 1);
    q_reg->ProbMaskAll(mask, probabilities);

    real1 expectation = calc_expectation(probabilities, feature_sv_size, eigenvalues);

    delete[] extended_input;
    delete[] input_state;
    delete[] probabilities;

    return expectation;
}

extern "C"
void run_inputs(
    float *expectations,
    float *inputs,
    float *thetas,
    int n_inputs,
    int input_size
) {
    // Pre-calc variables
    int n_feature_qubits = std::ceil(std::log2(input_size + 1)); // Add normalization bit
    int n_qubits = n_feature_qubits + 1;
    int feature_sv_size = (int) std::round(std::pow(2, n_feature_qubits));
    int sv_size = (int) std::round(std::pow(2, n_qubits));
    auto eigenvalues = new real1[feature_sv_size];
    for (int i = 0; i < feature_sv_size; i++) {
        eigenvalues[i] = get_eigenvalue(i);
    }

    #pragma omp parallel for default(none) shared( \
        n_inputs, expectations, input_size, inputs, thetas, eigenvalues, \
        n_feature_qubits, n_qubits, feature_sv_size, sv_size \
    ) num_threads(CORES)
    for (int i = 0; i < n_inputs; i++) {
        expectations[i] = run_circuit(
            input_size, &inputs[i * input_size], thetas, eigenvalues,
            n_feature_qubits, n_qubits, feature_sv_size, sv_size
        );
    }

    delete[] eigenvalues;
}


extern "C"
void run_thetas(
    float *expectations,
    float *inputs,
    float *thetas,
    int n_inputs,
    int input_size,
    int n_thetas
) {
    int n_feature_qubits = std::ceil(std::log2(input_size + 1)); // Add normalization bit
    int theta_size = n_feature_qubits * 3;

    float *theta_pointer = thetas;
    float *expectation_pointer = expectations;
    for (int i = 0; i < n_thetas; i++) {
        run_inputs(expectation_pointer, inputs, theta_pointer, n_inputs, input_size);
        theta_pointer += theta_size;
        expectation_pointer += n_inputs;
    }
}

int main() {
    int input_size = 9;
    auto *input = new float[input_size] { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9  };
    auto *thetas = new float[12] {
        -0.21107, 0.31022, -0.69577, -0.37123, 0.1, 0.2, -1.0, 0, -2.2, 0, 0, 0
    };

    // Pre-calc eigenvalues
    int n_feature_qubits = std::ceil(std::log2(input_size + 1)); // Add normalization bit
    int n_qubits = n_feature_qubits + 1;
    int feature_sv_size = (int) std::round(std::pow(2, n_feature_qubits));
    int sv_size = (int) std::round(std::pow(2, n_qubits));
    auto eigenvalues = new real1[sv_size];
    for (int i = 0; i < sv_size; i++) {
        eigenvalues[i] = get_eigenvalue(i);
    }


    for (int i = 0; i < 1000; i++)
        run_circuit(input_size, input, thetas, eigenvalues, n_feature_qubits, n_qubits, feature_sv_size, sv_size);

//    run_deutsch();

    delete[] input;
    delete[] thetas;
}
