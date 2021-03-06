/*
 *  ::718604!
 * 
 * Copyright(C) November 20, 2014 U.S. Food and Drug Administration
 * Authors: Dr. Vahan Simonyan (1), Dr. Raja Mazumder (2), et al
 * Affiliation: Food and Drug Administration (1), George Washington University (2)
 * 
 * All rights Reserved.
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*colset[0]=[1,3,6];
    colset[1]=[0,2,4,5];
           
    var  t=statTest(tblArr, colset);
    t.probvals[ir]>0.95
*/

// STATISTIC TEST: Student or ANOVAR
/**    actmat   : the table with rows and cols as the matrix: FORMAT [[],[]]
 *  colset   : groups of columns: FORMAT [[],[]]
 *  probvals : a float number
 *  tstatvals: a float number 
 */

function statTest(actmat, colset, probvals/*float*/,tstatvals /*float*/) // (actmat, colset, real * probvals, real * tstatvals)
{
    var sta=[{ss:0,average:0,n:0},{ss:0,average:0,n:0}];
    
    var ir, ic, is , grps, isT=0;

    grps = colset.length;
    
    if(grps==2) isT=1; 

    var result={probvals:[],tstatvals:[]};
    
    for ( ir=0; ir < actmat.rows.length ; ++ir ) { // loop rows
        var ssWAll=0, averageAll=0, ssBAll=0, prob, tval;
        var nAll=0;

        for( is=0; is<grps; ++is ) { // Loop through group
            var curGroup = colset[is]; // current group
            var groupLen = curGroup.length; // number of columns from that group

            var  average=0 , ss, s ; 
            for ( ic=0; ic<groupLen; ++ic) { // compute the AVERAGE of columns from the same group
                average += parseFloat(actmat.rows[ir].cols[curGroup[ic]]);
            }
            averageAll += average;
            average /= groupLen;
            ss=0;
            for ( ic=0; ic<groupLen; ++ic) { // compute the STDDEV
                s = parseFloat(actmat.rows[ir].cols[curGroup[ic]]) - average;
                ss += s*s;
            }
            ssWAll += ss;
            
            if(isT) {
                sta[is].ss = ss;
                sta[is].average = average;
                sta[is].n = groupLen;
            }
            nAll += groupLen;

            ssBAll += groupLen*average*average;
        }
        
        averageAll/=nAll;
        ssBAll-=nAll*averageAll*averageAll;

        if(isT) { // STUDENT
            var df = sta[0].n + sta[1].n -2;
            var sJoint = ssWAll/df;
            var t = sJoint ? (sta[0].average - sta[1].average)/Math.sqrt(sJoint*(1./sta[0].n+1./sta[1].n)) : 0 ;
            t = Math.abs(t);
            prob = studentsT(t, df );
            tval=t;
        }else { // ANOVA
            var dfb = grps-1;
            var dfw = nAll-grps;
            var MSB = ssBAll/dfb;
            var MSW = ssWAll/dfw;
            var f = MSB/MSW;
            prob = studentsF(f, dfb, dfw );
            tval=f;
        }
        result.probvals[ir]=prob;
        result.tstatvals[ir]=tval;
    }
    return result;
}


function studentsT(t /*float*/, df /*float*/)  
{
    return 1.0 - betaIncomplete(0.5*df, 0.5, df/(df+t*t));
}

function studentsF(f, df1, df2)
{
    var prob = betaIncomplete(0.5*df2, 0.5*df1, df2/(df2+df1*f));
    if (prob > 1.0) prob = 2.0 - prob;
    return prob;
}





// Returns the value ln[Gamma(xx)] for xx > 0.
function lnGamma(xx)
{
    var x, y, tmp, ser;
    var cof=[
        76.18009172947146,-86.50532032941677,
        24.01409824083091,-1.231739572450155,
        0.1208650973866179e-2,-0.5395239384953e-5
    ];

    y = x = xx;
    tmp = x+ 5.5;
    tmp -= (x+0.5)*Math.log(tmp);
    ser = 1.000000000190015;
    for (var j=0; j<=5; j++) 
        ser += cof[j]/++y;
    return -tmp + Math.log(2.5066282746310005*ser/x);
}

var REAL_MIN = 0;

var MAXIT = 1000;
var EPS = 3.0e-13;
var FPMIN = REAL_MIN;

// Used by betai: Evaluates continued fraction for incomplete beta function by modified Lentz�s method (section 5.2).
function betaContinuedFraction(a, b, x)
{
    var m,m2;
    var aa,c,d,del,h,qab,qam,qap;

    qab = a+b; // These q�s will be used in factors that occur in the coefcients (6.4.6). 
    qap = a + 1.0;
    qam = a - 1.0;
    c = 1.0; // First step of Lentz�s method.
    d = 1.0 - (qab*x)/qap;
    if (Math.abs(d) < FPMIN) d=FPMIN;
    d=1.0/d;
    h=d;
    for (m=1; m<=MAXIT; m++) {
        m2 = 2*m;
        aa = m*(b-m)*x/((qam+m2)*(a+m2));
        d = 1.0 + aa*d;  // One step (the even one) of the recurrence.
        if (Math.abs(d) < FPMIN) d=FPMIN;
        c = 1.0+aa/c;
        if (Math.abs(c) < FPMIN) c=FPMIN;
        d = 1.0/d;
        h *= d*c;
        aa = -(a+m)*(qab+m)*x/((a+m2)*(qap+m2));
        d = 1.0 + aa*d; // Next step of the recurrence (the odd one).
        if (Math.abs(d) < FPMIN) d=FPMIN;
        c = 1.0+aa/c;
        if (Math.abs(c) < FPMIN) c=FPMIN;
        d = 1.0/d;
        del = d*c;
        h *= del;
        if (Math.abs(del-1.0) < EPS) break; // Are we done?
    }
    // if (m > MAXIT) sFuncNRUtil::nrerror("a or b too big, or MAXIT too small in betacf");
    
    return h;
}

//Returns the incomplete beta function Ix(a, b).
function betaIncomplete(a /*float*/, b /*float*/, x /*float*/)
{
    var bt;
    if (x == 0.0 || x == 1.0){ 
        bt = 0.0;
    }else{ // Factors in front of the continued fraction.
        bt = Math.exp(lnGamma(a+b) - lnGamma(a) - lnGamma(b) + a*Math.log(x)+b*Math.log(1.0-x));
    }
    
    if (x < ((a+1.0)/(a+b+2.0)) ){  //Use continued fraction directly.
        return bt*betaContinuedFraction(a,b,x)/a;
    } else { // Use continued fraction after making the symmetry transformation. 
        return 1.0 - bt * (betaContinuedFraction(b,a,1.0-x))/b;
    }    
}


//# sourceURL = getBaseUrl() + "/js/math.js"