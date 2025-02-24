#cython: language_level=3
#cython: boundscheck=False
#cython: wraparound=False
#cython: cdivision=True

from cython.operator cimport preincrement as inc, predecrement as dec
from libc.stdlib cimport malloc, free


cdef extern from "gamma.h":
    cdef double lda_lgamma(double x) nogil
    cdef double lda_digamma(double x) nogil


cdef double lgamma(double x) nogil:
    if x <= 0:
        with gil:
            raise ValueError("x must be strictly positive")
    return lda_lgamma(x)


cdef double digamma(double x) nogil:
    return lda_digamma(x)
    

cdef int searchsorted(double* arr, int length, double value) nogil:
    """Bisection search (c.f. numpy.searchsorted)

    Find the index into sorted array `arr` of length `length` such that, if
    `value` were inserted before the index, the order of `arr` would be
    preserved.
    """
    cdef int imin, imax, imid
    imin = 0
    imax = length
    while imin < imax:
        imid = imin + ((imax - imin) >> 1)
        if value > arr[imid]:
            imin = imid + 1
        else:
            imax = imid
    return imin


def _sample_topics(int[:] WS, int[:] DS, int[:] ZS, int[:, :] nzw, int[:, :] ndz, int[:] nz, int[:] nd,
                   double[:] alpha, double eta, double[:] rands):
    cdef int i, k, w, d, z, z_new
    cdef double r, dist_cum
    cdef int N = WS.shape[0]
    cdef int D = ndz.shape[0]
    cdef int n_rand = rands.shape[0]
    cdef int n_topics = nz.shape[0]
    cdef int vocab_size = nzw.shape[1]
    cdef double* dist_sum = <double*> malloc(n_topics * sizeof(double))
    cdef double alpha_sum = 0
    cdef double psi_sum_nu = 0
    cdef double psi_sum_de = 0
    if dist_sum is NULL:
        raise MemoryError("Could not allocate memory during sampling.")
    with nogil:
        for i in range(alpha.shape[0]):
            alpha_sum += alpha[i]

        for i in range(N):
            w = WS[i]
            d = DS[i]
            z = ZS[i]

            dec(nzw[z, w])
            dec(ndz[d, z])
            dec(nz[z])

            dist_cum = 0
            for k in range(n_topics):
                # eta is a double so cdivision yields a double
                dist_cum += (nzw[k, w] + eta) / (nz[k] + eta * vocab_size) * (ndz[d, k] + alpha[k])
                dist_sum[k] = dist_cum

            r = rands[i % n_rand] * dist_cum  # dist_cum == dist_sum[-1]
            z_new = searchsorted(dist_sum, n_topics, r)

            ZS[i] = z_new
            inc(nzw[z_new, w])
            inc(ndz[d, z_new])
            inc(nz[z_new])

        free(dist_sum)

        # update hyperparameters
        for k in range(n_topics):
            psi_sum_nu = 0
            psi_sum_de = 0
            for d in range(D):
                psi_sum_nu += digamma(ndz[d, k] + alpha[k]) - digamma(alpha[k])
                psi_sum_de += digamma(nd[d] + alpha_sum) - digamma(alpha_sum)

            alpha[k] = alpha[k] * psi_sum_nu / psi_sum_de

        psi_sum_nu = 0
        psi_sum_de = 0

        for k in range(n_topics):
            for w in range(vocab_size):
                psi_sum_nu += digamma(nzw[k, w] + eta) - digamma(eta)

            psi_sum_de += digamma(nz[k] + eta * vocab_size) - digamma(eta * vocab_size)

        eta = eta * psi_sum_nu / (vocab_size * psi_sum_de)

    return alpha, eta
        

cpdef double _loglikelihood(int[:, :] nzw, int[:, :] ndz, int[:] nz, int[:] nd, double[:] alpha, double eta) nogil:
    cdef int k, d
    cdef int D = ndz.shape[0]
    cdef int n_topics = ndz.shape[1]
    cdef int vocab_size = nzw.shape[1]

    cdef double ll = 0

    # calculate log p(w|z)
    cdef double lgamma_eta
    cdef double alpha_sum = 0
    with nogil:
        lgamma_eta = lgamma(eta)
        for i in range(alpha.shape[0]):
            alpha_sum += alpha[i]

        ll += n_topics * lgamma(eta * vocab_size)
        for k in range(n_topics):
            ll -= lgamma(eta * vocab_size + nz[k])
            for w in range(vocab_size):
                # if nzw[k, w] == 0 addition and subtraction cancel out
                if nzw[k, w] > 0:
                    ll += lgamma(eta + nzw[k, w]) - lgamma_eta

        # calculate log p(z)
        for d in range(D):
            ll += (lgamma(alpha_sum) -
                    lgamma(alpha_sum + nd[d]))
            for k in range(n_topics):
                if ndz[d, k] > 0:
                    ll += lgamma(alpha[k] + ndz[d, k]) - lgamma(alpha[k])
        return ll
