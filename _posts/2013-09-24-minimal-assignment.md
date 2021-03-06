---
title: Munkres' Assignment Algorithm with RcppArmadillo
author: Lars Simon Zehnder
license: GPL (>= 2)
tags: armadillo
summary: Demonstrates the Minimal (or Maximal) Assignment Problem algorithm.
layout: post
src: 2013-09-24-minimal-assignment.cpp
---



_Munkres' Assignment Algorithm_ 
([Munkres (1957)](http://www.jstor.org/discover/10.2307/2098689?uid=3737864&uid=2&uid=4&sid=21102674250347),
also known as _hungarian algorithm_) is a well known algorithm 
in Operations Research solving the problem to optimally 
assign `N` jobs to `N` workers. 

I needed to solve the _Minimal Assignment Problem_ for a 
relabeling algorithm in MCMC sampling for _finite mixture_
distributions, where I use a random permutation Gibbs 
sampler. For each sample an optimal labeling must be found,
i.e. I have `N` parameter vectors and must assign each vector
to one of the `N` components of the finite mixture under the restriction
that each component gets a parameter vector. For the assignment
of parameters to components 
[Stephens (1997b)] (http://www.isds.duke.edu/~scs/Courses/Stat376/Papers/Mixtures/LabelSwitchingStephensJRSSB.pdf) 
suggests an algorithm relying on the Minimal Assignment in regard
to a _loss matrix_. The labeling with the smallest loss is
then considered as optimal.

After an unsuccessful search for libraries implementing 
the algorithm easily for C++ or C, I made the decision to
program it myself using `RcppArmadillo` for good performance. 
I found some guidance by the websites of 
[Bob Pilgrim](http://csclab.murraystate.edu/bob.pilgrim/445/munkres.html) and 
[TopCoder] (http://community.topcoder.com/tc?module=Static&d1=tutorials&d2=hungarianAlgorithm).
These websites offer excellent tutorials to understand 
the algorithm and to convert it to code. The order of this 
implementation of Munkres' algorithm is of an order `N` to the power of 4. There exists
also a version of order `N` to the power of 3, but an order of `N` to the power of 4 works 
very good for me and coding time is as usual a critical factor
for me.

In the following I walk through the different steps of
Munkres' algorithm and explain the main parts and their
functionality.

Let's begin. The first step in Munkres' algorithm is to 
subtract the minimal element of each row from each element 
in this row.  

{% highlight cpp %}
#include<RcppArmadillo.h>
// [[Rcpp::depends(RcppArmadillo)]]

void step_one(unsigned int &step, arma::mat &cost, 
        const unsigned int &N) 
{    
    for (unsigned int r = 0; r < N; ++r) {
        cost.row(r) -= arma::min(cost.row(r));
    }
    step = 2;
}
{% endhighlight %}


Note, that we use references for all function arguments.
As we have to switch between the steps of the algorithm
continously, we always must be able to determine which 
step should be chosen next. Therefore we give a mutable
unsigned integer `step` as an argument to each step
function of the algorithm. 

Inside the function we can easily access a whole row by 
Armadillo's `row()` method for matrices.
In the second step, we then search for a zero in the
modified cost matrix of step one. 

{% highlight cpp %}
void step_two (unsigned int &step, const arma::mat &cost,
        arma::umat &indM, arma::ivec &rcov, 
        arma::ivec &ccov, const unsigned int &N)
{                      
    for (unsigned int r = 0; r < N; ++r) {
        for (unsigned int c = 0; c < N; ++c) {
            if (cost.at(r, c) == 0.0 && rcov.at(r) == 0 && ccov.at(c) == 0) {
                indM.at(r, c)  = 1;
                rcov.at(r)     = 1;
                ccov.at(c)     = 1;
                break;                                              // Only take the first
                                                                    // zero in a row and column
            }
        }
    }
    /* for later reuse */
    rcov.fill(0);
    ccov.fill(0);
    step = 3;
}
{% endhighlight %}


Only the first zero in a row is taken. Then, the indicator
matrix `indM` indicates this zero by setting the corresponding
element at `(r, c)` to 1. A unique zero - the only or first one in
a column and row - is called _starred zero_. In `step 2` we find
such a _starred zero_.

Note, that we use here Armadillo's element access via the 
method `at()`, which makes no bound checks and improves performance.

_Note Bene: This code is thoroughly debugged - never do this for fresh written
code!_

In `step 3` we cover each column with a _starred zero_. If already 
`N` columns are covered all _starred zeros_ describe a complete
assignment - so, go to `step 7` and finish. Otherwise go to
`step 4`.

{% highlight cpp %}
void step_three(unsigned int &step, const arma::umat &indM,
        arma::ivec &ccov, const unsigned int &N) 
{
    unsigned int colcount = 0;
    for (unsigned int r = 0; r < N; ++r) {
        for (unsigned int c = 0; c < N; ++c) {
            if (indM.at(r, c) == 1) {
                ccov.at(c) = 1;
            }
        }
    }
    for (unsigned int c = 0; c < N; ++c) {
        if (ccov.at(c) == 1) {
            ++colcount;
        }
    }
    if (colcount == N) {
        step = 7;
    } else {
        step = 4;
    }
}
{% endhighlight %}


We cover a column by looking for 1s in the indicator 
matrix `indM` (See `step 2` for assuring that these are
indeed only _starred zeros_).

`Step 4` finds _noncovered zeros_ and _primes_ them. If there
are zeros in a row and none of them is _starred_, _prime_
them. For this task we program a helper function to keep
the code more readable and reusable. The helper function
searches for _noncovered zeros_.

{% highlight cpp %}
void find_noncovered_zero(int &row, int &col,
        const arma::mat &cost, const arma::ivec &rcov, 
        const arma::ivec &ccov, const unsigned int &N)
{
    unsigned int r = 0;
    unsigned int c;
    bool done = false;
    row = -1;
    col = -1;
    while (!done) {
        c = 0;
        while (true) {
            if (cost.at(r, c) == 0.0 && rcov.at(r) == 0 && ccov.at(c) == 0) {
                row = r;
                col = c;
                done = true;
            }
            ++c;
            if (c == N || done) {
                break;
            }
        }
        ++r;
        if (r == N) {
            done = true;
        }
    }
}
{% endhighlight %}


We can detect _noncovered zeros_ by checking if the cost matrix
contains at row r and column c a zero and row and column
are not covered yet, i.e. `rcov(r) == 0`, `ccov(c) == 0`. 
This loop breaks, if we have found our first _uncovered zero_  or
no _uncovered zero_ at all. 

In `step 4`, if no _uncovered zero_ is found we go to `step 6`. If
instead an _uncovered zero_ has been found, we set the indicator 
matrix at its position to 2. We then have to search for a _starred 
zero_ in the row with the _uncovered zero_, _uncover_ the column with
the _starred zero_ and _cover_ the row with the _starred zero_. To 
indicate a _starred zero_ in a row and to find it we create again 
two helper functions.

{% highlight cpp %}
bool star_in_row(int &row, const arma::umat &indM,
        const unsigned int &N) 
{
    bool tmp = false;
    for (unsigned int c = 0; c < N; ++c) {
        if (indM.at(row, c) == 1) {
            tmp = true;
            break;
        }
    }
    return tmp;
}

void find_star_in_row (const int &row, int &col, 
        const arma::umat &indM, const unsigned int &N) 
{
    col = -1;
    for (unsigned int c = 0; c < N; ++c) {
        if (indM.at(row, c) == 1) {
            col = c;
        }
    }
}
{% endhighlight %}


We know that _starred zeros_ are indicated by the indicator
matrix containing an element equal to 1. 
Now, `step 4`.

{% highlight cpp %}
void step_four (unsigned int &step, const arma::mat &cost,
        arma::umat &indM, arma::ivec &rcov, arma::ivec &ccov,
        int &rpath_0, int &cpath_0, const unsigned int &N) 
{
    int row = -1;
    int col = -1;
    bool done = false;
    while(!done) {
        find_noncovered_zero(row, col, cost, rcov,
                ccov, N);                                 
                                                        
        if (row == -1) {                                
            done = true;
            step = 6;
        } else {
            /* uncovered zero */
            indM(row, col) = 2;                         
            if (star_in_row(row, indM, N)) {                            
                find_star_in_row(row, col, indM, N);    
                /* Cover the row with the starred zero
                 * and uncover the column with the starred
                 * zero. 
                 */
                rcov.at(row) = 1;                         
                ccov.at(col) = 0;                          
            } else {
                /* No starred zero in row with 
                 * uncovered zero 
                 */
                done = true;
                step = 5;
                rpath_0 = row;
                cpath_0 = col;
            }            
        }
    }
}
{% endhighlight %}


Notice the `rpath_0` and `cpath_0` variables. These integer
variables store the first _vertex_ for an _augmenting path_ in `step 5`.
If zeros could be _primed_ we go further to `step 5`. 

`Step 5` constructs a path beginning at an _uncovered primed
zero_ (this is actually graph theory - alternating and augmenting 
paths) and alternating between _starred_ and _primed zeros_. 
This path is continued until a _primed zero_ with no _starred 
zero_ in its column is found. Then, all _starred zeros_ in 
this path are _unstarred_ and all _primed zeros_ are _starred_. 
All _primes_ in the indicator matrix are erased and all rows
are _uncovered_. Then return to `step 3` to _cover_ again columns.

`Step 5` needs several helper functions. First, we need
a function to find _starred zeros_ in columns. 

{% highlight cpp %}
void find_star_in_col (const int &col, int &row,
        const arma::umat &indM, const unsigned int &N)
{
    row = -1;
    for (unsigned int r = 0; r < N; ++r) {
        if (indM.at(r, col) == 1) {
            row = r;
        }
    }
}
{% endhighlight %}


Then we need a function to find a _primed zero_ in a row.
Note, that these tasks are easily performed by searching the 
indicator matrix `indM`. 

{% highlight cpp %}
void find_prime_in_row (const int &row, int &col,
        const arma::umat &indM, const unsigned int &N)
{
    for (unsigned int c = 0; c < N; ++c) {
        if (indM.at(row, c) == 2) {
            col = c;
        }
    }
}
{% endhighlight %}


In addition we need a function to augment the path, one to
clear the _covers_ from rows and one to erase the _primed zeros_ 
from the indicator matrix `indM`. 

{% highlight cpp %}
void augment_path (const int &path_count, arma::umat &indM,
        const arma::imat &path)
{
    for (unsigned int p = 0; p < path_count; ++p) {
        if (indM.at(path(p, 0), path(p, 1)) == 1) {
            indM.at(path(p, 0), path(p, 1)) = 0;
        } else {
            indM.at(path(p, 0), path(p, 1)) = 1;
        }
    }
}

void clear_covers (arma::ivec &rcov, arma::ivec &ccov)
{
    rcov.fill(0);
    ccov.fill(0);
}

void erase_primes(arma::umat &indM, const unsigned int &N)
{
    for (unsigned int r = 0; r < N; ++r) {
        for (unsigned int c = 0; c < N; ++c) {
            if (indM.at(r, c) == 2) {
                indM.at(r, c) = 0;
            }
        }
    }
}
{% endhighlight %}


The function to augment the path gets an integer matrix `path`
of dimension 2 * N x 2. In it all vertices between rows and columns
are stored row-wise. 
Now, we can set the complete `step 5`:

{% highlight cpp %}
void step_five (unsigned int &step,
        arma::umat &indM, arma::ivec &rcov, 
        arma::ivec &ccov, arma::imat &path, 
        int &rpath_0, int &cpath_0, 
        const unsigned int &N)
{
    bool done = false;
    int row = -1;
    int col = -1;
    unsigned int path_count = 1;
    path.at(path_count - 1, 0) = rpath_0;
    path.at(path_count - 1, 1) = cpath_0;
    while (!done) {
        find_star_in_col(path.at(path_count - 1, 1), row, 
                indM, N);
        if (row > -1) {                                
            /* Starred zero in row 'row' */
            ++path_count;
            path.at(path_count - 1, 0) = row;
            path.at(path_count - 1, 1) = path.at(path_count - 2, 1);
        } else {
            done = true;
        }
        if (!done) {
            /* If there is a starred zero find a primed 
             * zero in this row; write index to 'col' */
            find_prime_in_row(path.at(path_count - 1, 0), col, 
                    indM, N);  
            ++path_count;
            path.at(path_count - 1, 0) = path.at(path_count - 2, 0);
            path.at(path_count - 1, 1) = col;
        }
    }
    augment_path(path_count, indM, path);
    clear_covers(rcov, ccov);
    erase_primes(indM, N);
    step = 3;
}
{% endhighlight %}


Recall, if `step 4` was successfull in uncovering all columns 
and covering all rows with a primed zero, it then calls
`step 6`. 
`Step 6` takes the cover vectors `rcov` and `ccov` and looks 
in the uncovered region of the cost matrix for the smallest
value. It then subtracts this value from each element in an
_uncovered column_ and adds it to each element in a _covered row_. 
After this transformation, the algorithm starts again at `step 4`. 
Our last helper function searches for the smallest value in 
the uncovered region of the cost matrix.

{% highlight cpp %}
void find_smallest (double &minval, const arma::mat &cost, 
        const arma::ivec &rcov, const arma::ivec &ccov, 
        const unsigned int &N)
{
    for (unsigned int r = 0; r < N; ++r) {
        for (unsigned int c = 0; c < N; ++c) {
            if (rcov.at(r) == 0 && ccov.at(c) == 0) {                                                                    
                if (minval > cost.at(r, c)) {
                    minval = cost.at(r, c);
                }
            }
        }
    }
}
{% endhighlight %}


`Step 6` looks as follows:

{% highlight cpp %}
void step_six (unsigned int &step, arma::mat &cost,
        const arma::ivec &rcov, const arma::ivec &ccov, 
        const unsigned int &N) 
{
    double minval = DBL_MAX;
    find_smallest(minval, cost, rcov, ccov, N);
    for (unsigned int r = 0; r < N; ++r) {
        for (unsigned int c = 0; c < N; ++c) {
            if (rcov.at(r) == 1) {
                cost.at(r, c) += minval;
            }
            if (ccov.at(c) == 0) {
                cost.at(r, c) -= minval;
            }
        }
    }
    step = 4;
}
{% endhighlight %}


At last, we must create a function that enables us to 
jump around the different steps of the algorithm. 
The following code shows the main function of 
the algorithm. It defines also the important variables
to be passed to the different steps.

{% highlight cpp %}
arma::umat hungarian(const arma::mat &input_cost)
{
    const unsigned int N = input_cost.n_rows;
    unsigned int step = 1;
    int cpath_0 = 0;
    int rpath_0 = 0;
    arma::mat cost(input_cost);
    arma::umat indM(N, N);
    arma::ivec rcov(N);
    arma::ivec ccov(N);
    arma::imat path(2 * N, 2);

    indM = arma::zeros<arma::umat>(N, N);
    bool done = false;
    while (!done) {
        switch (step) {
            case 1:
                step_one(step, cost, N);
                break;
            case 2:
                step_two(step, cost, indM, rcov, ccov, N);
                break;
            case 3:
                step_three(step, indM, ccov, N);
                break;
            case 4:
                step_four(step, cost, indM, rcov, ccov,
                        rpath_0, cpath_0, N);
                break;
            case 5: 
                step_five(step, indM, rcov, ccov,
                        path, rpath_0, cpath_0, N);
                break;
            case 6:
                step_six(step, cost, rcov, ccov, N);
                break;
            case 7:            
                done = true;
                break;
        }
    }
    return indM;
}
{% endhighlight %}


Note, this function takes the numeric cost matrix as
an argument and returns the integer indicator matrix 
`indM`. 

I chose to set the argument `input_cost` constant and copy
it for reasons of reusability of the cost matrix in other
C++ code. When working with rather huge cost matrices, it
makes sense to make the argument mutable. Though, I used 
_pass-by-reference_ for all the arguments in functions to 
avoid useless copying inside the functions. 

To call the main function `hungarian` from R and to use 
our algorithm we construct an `Rcpp Attribute`: 

{% highlight cpp %}
// [[Rcpp::export]]

arma::imat hungarian_cc(Rcpp::NumericMatrix cost)
{
    // Reuse memory from R
    unsigned int N = cost.rows();
    arma::mat arma_cost(cost.begin(), N, N, false, true);
    // Call the C++-function 'hungarian'
    arma::umat indM = hungarian(arma_cost);
    //Convert the result to an Armadillo integer
    //matrix - R does not know unsigned integers.
    return arma::conv_to<arma::imat>::from(indM);
}
{% endhighlight %}


If we want to provide this function also to other users
we should probably ensure, that the matrix is square (There 
exists also an extension to rectangular matrices, see
[Burgeois and Lasalle (1971)](http://dl.acm.org/citation.cfm?id=362945)).
This can be done easily with the exceptions provided by 
`Rcpp` passed over to R:

{% highlight cpp %}
// [[Rcpp::export]]
arma::imat hungariansafe_cc(Rcpp::NumericMatrix cost)
{
    unsigned int N = cost.rows();
    unsigned int K = cost.cols();
    if (N != K) {
        throw Rcpp::exception("Matrix is not square.");
    }
    // Reuse memory from R
    arma::mat arma_cost(cost.begin(), N, K, false, true);
    // Call the C++-function 'hungarian'
    arma::umat indM = hungarian(arma_cost);
    //convert the result to an Armadillo integer
    //matrix - R does not know unsigned integers.
    return arma::conv_to<arma::imat>::from(indM);
}
{% endhighlight %}


Note, that it is also possible to use for the attribute 
directly an `Armadillo` matrix, but following the [recent 
discussion on the Rcpp-devel list](http://www.mail-archive.com/rcpp-devel@lists.r-forge.r-project.org/msg05784.html),
a _pass-by-reference_ of arguments is not yet possible. Romain
Francois' proposals seem promising, so maybe we can expect
in some of the next releases _shallow_ copies allowing 
_pass-by-reference_ in `Rcpp Attributes`. 

Let us begin now with a very easy example that makes clear
what is going on. 

{% highlight r %}
# Check exception:
cost <- matrix(c(1:6), nrow = 3, ncol = 2, byrow = TRUE)
tryCatch(indM <- hungariansafe_cc(cost), error = function(e) {print(e)})
{% endhighlight %}



<pre class="output">
&lt;Rcpp::exception: Matrix is not square.&gt;
</pre>



{% highlight r %}
cost <- matrix(c(1, 2, 2, 4), nrow = 2, ncol = 2, byrow = TRUE)
cost
{% endhighlight %}



<pre class="output">
     [,1] [,2]
[1,]    1    2
[2,]    2    4
</pre>



{% highlight r %}
indM <- hungarian_cc(cost)
indM
{% endhighlight %}



<pre class="output">
     [,1] [,2]
[1,]    0    1
[2,]    1    0
</pre>



{% highlight r %}
min.cost <- sum(indM * cost)
min.cost
{% endhighlight %}



<pre class="output">
[1] 4
</pre>


We can also compute a maximal assignment over a revenue 
matrix by simply considering the difference between 
a big value and this matrix as a cost matrix. 

{% highlight r %}
revenues <- matrix(seq(1, 4)) %*% seq(1, 4)
revenues
{% endhighlight %}



<pre class="output">
     [,1] [,2] [,3] [,4]
[1,]    1    2    3    4
[2,]    2    4    6    8
[3,]    3    6    9   12
[4,]    4    8   12   16
</pre>



{% highlight r %}
cost <- 100 - revenues
indM <- hungarian_cc(cost)
indM
{% endhighlight %}



<pre class="output">
     [,1] [,2] [,3] [,4]
[1,]    1    0    0    0
[2,]    0    1    0    0
[3,]    0    0    1    0
[4,]    0    0    0    1
</pre>



{% highlight r %}
max.revenue <- sum(indM * revenues)
max.revenue
{% endhighlight %}



<pre class="output">
[1] 30
</pre>


CoCost matrices containing negative values work as well:

{% highlight r %}
cost <- matrix(rnorm(100), ncol = 10, nrow = 10)
cost
{% endhighlight %}



<pre class="output">
          [,1]    [,2]    [,3]     [,4]     [,5]     [,6]    [,7]
 [1,] -0.23018  0.3598 -0.2180 -0.29507 -0.20792 -0.02855 -0.5023
 [2,]  1.55871  0.4008 -1.0260  0.89513 -1.26540 -0.04287 -0.3332
 [3,]  0.07051  0.1107 -0.7289  0.87813  2.16896  1.36860 -1.0186
 [4,]  0.12929 -0.5558 -0.6250  0.82158  1.20796 -0.22577 -1.0718
 [5,]  1.71506  1.7869 -1.6867  0.68864 -1.12311  1.51647  0.3035
 [6,]  0.46092  0.4979  0.8378  0.55392 -0.40288 -1.54875  0.4482
 [7,] -1.26506 -1.9666  0.1534 -0.06191 -0.46666  0.58461  0.0530
 [8,] -0.68685  0.7014 -1.1381 -0.30596  0.77997  0.12385  0.9223
 [9,] -0.44566 -0.4728  1.2538 -0.38047 -0.08337  0.21594  2.0501
[10,]  1.22408 -1.0678  0.4265 -0.69471  0.25332  0.37964 -0.4910
           [,8]    [,9]   [,10]
 [1,] -2.309169  0.3853  0.5484
 [2,]  1.005739 -0.3707  0.2387
 [3,] -0.709201  0.6444 -0.6279
 [4,] -0.688009 -0.2205  1.3607
 [5,]  1.025571  0.3318 -0.6003
 [6,] -0.284773  1.0968  2.1873
 [7,] -1.220718  0.4352  1.5326
 [8,]  0.181303 -0.3259 -0.2357
 [9,] -0.138891  1.1488 -1.0264
[10,]  0.005764  0.9935 -0.7104
</pre>



{% highlight r %}
indM <- hungarian_cc(cost)
indM
{% endhighlight %}



<pre class="output">
      [,1] [,2] [,3] [,4] [,5] [,6] [,7] [,8] [,9] [,10]
 [1,]    0    0    0    0    0    0    0    1    0     0
 [2,]    0    0    0    0    1    0    0    0    0     0
 [3,]    0    0    0    0    0    0    1    0    0     0
 [4,]    0    0    0    0    0    0    0    0    1     0
 [5,]    0    0    1    0    0    0    0    0    0     0
 [6,]    0    0    0    0    0    1    0    0    0     0
 [7,]    0    1    0    0    0    0    0    0    0     0
 [8,]    1    0    0    0    0    0    0    0    0     0
 [9,]    0    0    0    0    0    0    0    0    0     1
[10,]    0    0    0    1    0    0    0    0    0     0
</pre>



{% highlight r %}
min.cost <- sum(indM * cost)
min.cost
{% endhighlight %}



<pre class="output">
[1] -12.42
</pre>


Finally let us make some benchmarking. We load the 
rbenchmark package and take now a more _realistic_ cost
matrix.

{% highlight r %}
library(rbenchmark)
cost <- matrix(rpois(10000, 312), ncol = 100, nrow = 100)
benchmark(indM <- hungarian_cc(cost), columns=c('test','replications','elapsed','user.self','sys.self'), replications=1000)
{% endhighlight %}



<pre class="output">
                        test replications elapsed user.self sys.self
1 indM &lt;- hungarian_cc(cost)         1000   10.53      10.5    0.004
</pre>


But we also see, where the limitations of this algorithm lie
- huge matrices:

{% highlight r %}
cost <- matrix(rpois(250000, 312), ncol = 500, nrow = 500)
system.time(indM <- hungarian_cc(cost))
{% endhighlight %}



<pre class="output">
   user  system elapsed 
  3.656   0.008   3.674 
</pre>


Some last notes on the structure of the code. I prefer to
put the code of the algorithm in an header file, e.g.
`hungarian.h`. And use an `attributes.cpp` (`attributes.cc`)
file to program the `Rcpp Attributes`. With this, 
I can easily reuse the algorithm in C++ code by simple
inclusion (`#include "hungarian.h"`) and have a complete 
overview on all the C++ functions I export to R.
