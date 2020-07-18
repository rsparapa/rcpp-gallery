/**
 * @title Accessing entries of a three-dimensional array
 * @author Rodney Sparapani
 * @license GPL (>= 2)
 * @tags array
 * @summary An example of how to access the entries of an array 
 *   since generic (i, j, k)-like operators don't exist.
 */

#include <Rcpp.h>
// [[Rcpp::export]]
using namespace Rcpp;
IntegerVector get3d(IntegerVector x, IntegerVector args) {
  IntegerVector rc(1), dim=x.attr("dim");
  rc[0]=R_NaN;
  const size_t K=args.size();
  if(K!=dim.size()) return rc;
  size_t i;
  if(K==1) {
    i=args[0]-1;
    if(i>=0 && i<dim[0]) rc[0]=x[i];
  }
  else if(K==2) {
    i=(args[1]-1)*dim[0]+args[0]-1;
    if(i>=0 && i<(dim[0]*dim[1])) rc[0]=x[i];
  }
  else if(K==3) {
    i=((args[2]-1)*dim[1]+(args[1]-1))*dim[0]+args[0]-1;
    if(i>=0 && i<(dim[0]*dim[1]*dim[2])) rc[0]=x[i];
  }
  return rc;
}

/**
 * This function returns the entry corresponding to "args" from "x" which 
 * is either a 1, 2 or 3-dimensional arrays.  Since the `(i, j, k)` operator
 * doesn't exist, we resort to "args" which is an integer vector.
 */

/*** R
library(Rcpp)

b = array(1:8, dim=8)
c = array(1:8, dim=c(2, 4))
a <- array(1:24, dim=c(2, 3, 4))

get3d(b, 3)
b[3]
for(k in 1:8) print(b[k]-get3d(b, k))

get3d(c, c(2, 1))
c[2, 1]
for(i in 1:2)
        for(k in 1:4) print(c[i, k]-get3d(c, c(i, k)))

get3d(a, c(2, 1, 1))
a[2, 1, 1]

for(i in 1:2)
    for(j in 1:3)
        for(k in 1:4) 
            print(a[i, j, k]-get3d(a, c(i, j, k)))
*/
