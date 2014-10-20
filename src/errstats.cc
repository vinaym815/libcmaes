/**
 * CMA-ES, Covariance Matrix Adaptation Evolution Strategy
 * Copyright (c) 2014 INRIA
 * Author: Emmanuel Benazera <emmanuel.benazera@lri.fr>
 *
 * This file is part of libcmaes.
 *
 * libcmaes is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libcmaes is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libcmaes.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "errstats.h"
#include <llogging.h>
#include <iostream>

namespace libcmaes
{
  
  template <class TGenoPheno>
  pli errstats<TGenoPheno>::profile_likelihood(FitFunc &func,
					       const CMAParameters<TGenoPheno> &parameters,
					       CMASolutions &cmasol,
					       const int &k,
					       const bool &curve,
					       const int &samplesize,
					       const double &fup,
					       const double &delta)
  {
    dVec x = cmasol.best_candidate().get_x_dvec();
    double minfvalue = cmasol.best_candidate().get_fvalue();
    dVec phenox = parameters.get_gp().pheno(x);
    
    //debug
    //std::cout << "xk=" << x[k] << " / minfvalue=" << minfvalue << std::endl;
    //std::cout << "phenox=" << phenox << std::endl;
    //debug

    pli le(k,samplesize,parameters.dim(),parameters.get_gp().pheno(x),minfvalue,fup,delta);

    errstats<TGenoPheno>::profile_likelihood_search(func,parameters,le,cmasol,k,false,samplesize,fup,delta,curve); // positive direction
    errstats<TGenoPheno>::profile_likelihood_search(func,parameters,le,cmasol,k,true,samplesize,fup,delta,curve);  // negative direction
    
    le.setErrMinMax();
    cmasol._pls.insert(std::pair<int,pli>(k,le));
    return le;
  }

  template <class TGenoPheno>
  void errstats<TGenoPheno>::profile_likelihood_search(FitFunc &func,
						       const CMAParameters<TGenoPheno> &parameters,
						       pli &le,
						       const CMASolutions &cmasol,
						       const int &k,
						       const bool &neg,
						       const int &samplesize,
						       const double &fup,
						       const double &delta,
						       const bool &curve)
  {
    int sign = neg ? -1 : 1;
    dVec x = cmasol.best_candidate().get_x_dvec();
    double xk = x[k];
    double minfvalue = cmasol.best_candidate().get_fvalue();
    CMASolutions citsol = cmasol;
    double dxk = sign * xk * 0.1;
    for (int i=0;i<samplesize;i++)
      {
	// get a new xk point.
	bool iterend = errstats<TGenoPheno>::take_linear_step(func,parameters,k,minfvalue,fup,curve,x,dxk);
		
	//debug
	//std::cout << "new xk point: " << x.transpose() << std::endl;
	//debug
	
	// minimize.
	CMASolutions ncitsol = errstats<TGenoPheno>::optimize_pk(func,parameters,citsol,k,x[k]);
	if (ncitsol._run_status < 0)
	  {
	    LOG(WARNING) << "profile likelihood linesearch: optimization error " << ncitsol._run_status << std::endl;
	    // pad and return.
	    /*for (int j=i+1;j<samplesize;j++)
	      {
		le._fvaluem[samplesize+sign*(1+j)] = le._fvaluem[samplesize+sign*i];
		le._xm.row(samplesize+sign*(1+j)) = le._xm.row(samplesize+sign*i);
	      }
	      return;*/
	  }
	else // update current point and solution.
	  {
	    citsol = ncitsol;
	    x = citsol.best_candidate().get_x_dvec();
	    minfvalue = citsol.best_candidate().get_fvalue();
	  }
	
	// store points.
	dVec phenobx = parameters.get_gp().pheno(citsol.best_candidate().get_x_dvec());
	le._fvaluem[samplesize+sign*(1+i)] = citsol.best_candidate().get_fvalue();
	le._xm.row(samplesize+sign*(1+i)) = phenobx.transpose();
	le._err[samplesize+sign*(1+i)] = ncitsol._run_status;
	
	if (!curve && iterend)
	  {
	    // pad and return.
	    for (int j=i+1;j<samplesize;j++)
	      {
		le._fvaluem[samplesize+sign*(1+j)] = citsol.best_candidate().get_fvalue();
		le._xm.row(samplesize+sign*(1+j)) = phenobx.transpose();
		le._err[samplesize+sign*(1+j)] = ncitsol._run_status;
	      }
	    return;
	  }
      }
  }
						
  template <class TGenoPheno>
  bool errstats<TGenoPheno>::take_linear_step(FitFunc &func,
					      const CMAParameters<TGenoPheno> &parameters,
					      const int &k,
					      const double &minfvalue,
					      const double &fup,
					      const bool &curve,
					      dVec &x,
					      double &dxk)
  {
    static double fdiff_relative_increase = 0.1;
    double fdelta = 0.1 * fup;
    double threshold = minfvalue + fup;
    dVec xtmp = x;
    xtmp[k] += dxk;
    double fvalue = func(parameters.get_gp().pheno(xtmp).data(),xtmp.size());
    double fdiff = fvalue - minfvalue;
    dVec phenoxtmp = parameters.get_gp().pheno(xtmp);
    
    //debug
    /*std::cout << "xtmp=" << xtmp.transpose() << std::endl;
    std::cout << "phenoxtmp=" << phenoxtmp.transpose() << std::endl;
    std::cout << "dxk=" << dxk << " / threshold=" << threshold << " / fvalue=" << fvalue << " / fdiff=" << fdiff << " / fabs=" << fabs(fvalue-fup) << " / fdelta=" << fdelta << std::endl;*/
    //debug

    if (fdiff > threshold * fdiff_relative_increase) // decrease dxk
      {
	while((curve || fabs(fvalue-fup)>fdelta)
	      && fdiff > threshold * fdiff_relative_increase
	      && phenoxtmp[k] >= parameters.get_gp().get_boundstrategy_const().getPhenoLBound(k))
	  {
	    //std::cerr << "fvalue=" << fvalue << " / xtmpk=" << phenoxtmp[k] << " / lbound=" << parameters.get_gp()._boundstrategy.getLBound(k) << std::endl;
	    dxk /= 2.0;
	    xtmp[k] = x[k] + dxk;
	    phenoxtmp = parameters.get_gp().pheno(xtmp);
	    fvalue = func(phenoxtmp.data(),xtmp.size());
	    fdiff = fvalue - minfvalue;
	  }
      }
    else // increase dxk
      {
	while ((curve || fabs(fvalue-fup)>fdelta)
	       && fdiff < threshold * fdiff_relative_increase
	       && phenoxtmp[k] <= parameters.get_gp().get_boundstrategy_const().getPhenoUBound(k))
	  {
	    //std::cerr << "fvalue=" << fvalue << " / xtmpk=" << phenoxtmp[k] << " / ubound=" << parameters.get_gp()._boundstrategy.getUBound(k) << std::endl;
	    dxk *= 2.0;
	    xtmp[k] = x[k] + dxk;
	    phenoxtmp =parameters.get_gp().pheno(xtmp);
	    fvalue = func(phenoxtmp.data(),xtmp.size());
	    fdiff = fvalue - minfvalue;
	  }
	dxk /= 2.0;
      }
    x[k] = xtmp[k]; // set value.
    dVec phenox = parameters.get_gp().pheno(x);
    return (fabs(fvalue-fup) < fdelta || phenox[k] < parameters.get_gp().get_boundstrategy_const().getPhenoLBound(k) || phenox[k] > parameters.get_gp().get_boundstrategy_const().getPhenoUBound(k));
  }

  template <class TGenoPheno>
  CMASolutions errstats<TGenoPheno>::optimize_pk(FitFunc &func,
						 const CMAParameters<TGenoPheno> &parameters,
						 const CMASolutions &cmasol,
						 const int &k,
						 const double &vk)
  {
    CMASolutions ncmasol = cmasol;
    CMAParameters<TGenoPheno> nparameters = parameters;
    nparameters.set_quiet(true); //TODO: option.
    nparameters.set_fixed_p(k,vk);
    ncmasol.set_sigma(fabs(cmasol.best_candidate().get_x()[k]-vk));
    nparameters.set_sigma_init(ncmasol.sigma());
    return cmaes(func,nparameters,CMAStrategy<CovarianceUpdate,TGenoPheno>::_defaultPFunc,nullptr,ncmasol); //TODO: explicitely set the initial covariance.
  }
    
  template <class TGenoPheno>
  CMASolutions errstats<TGenoPheno>::optimize_reduced_pk(FitFunc &func,
							 CMAParameters<TGenoPheno> &parameters,
							 const CMASolutions &cmasol,
							 const int &k,
							 const double &vk)
  {
    // set re-arranged solution as starting point of the new optimization problem.
    CMASolutions ncmasol = cmasol;
    ncmasol.reset_as_fixed(k);

    // re-arrange function
    FitFunc nfunc = [func,k,vk](const double *x, const int N)
      {
	std::vector<double> nx;
	nx.reserve(N+1);
	for (int i=0;i<N+1;i++)
	  {
	    if (i < k)
	      nx.push_back(x[i]);
	    else if (i == k)
	      nx.push_back(vk);
	    else nx.push_back(x[i-1]);
	  }
	return func(&nx.front(),N+1);
      };
    
    // optimize and return result.
    CMAParameters<TGenoPheno> nparameters = parameters;
    nparameters.reset_as_fixed(k);
    return cmaes(nfunc,nparameters);
  }

  template <class TGenoPheno>
  contour errstats<TGenoPheno>::contour_points(FitFunc & func, const int &px, const int &py, const int &npoints,
					       const CMAParameters<TGenoPheno> &parameters,
					       CMASolutions &cmasol)
  {
    // find first two points.
    pli plx,ply;
    if (!cmasol.get_pli(px,plx))
      {
	errstats<TGenoPheno>::profile_likelihood(func,parameters,cmasol,px); // use default parameters.
	cmasol.get_pli(px,plx);
      }
    
    // find second two points.
    if (!cmasol.get_pli(py,ply))
      {
	errstats<TGenoPheno>::profile_likelihood(func,parameters,cmasol,py); // use default parameters.
	cmasol.get_pli(py,ply);
      }

    double valx = cmasol.best_candidate().get_x()[px];
    double valy = cmasol.best_candidate().get_x()[py];
    
    // find upper y value for x parameter.
    CMAParameters<TGenoPheno> nparameters = parameters;
    nparameters.set_fixed_p(px,valx+plx._errmax);
    CMASolutions exy_up = cmaes(func,nparameters);
    
    // find lower y value for x parameter.
    nparameters = parameters;
    nparameters.set_fixed_p(px,valx+plx._errmin);
    CMASolutions exy_lo = cmaes(func,nparameters);
    
    // find upper x value for y parameter.
    nparameters = parameters;
    nparameters.set_fixed_p(py,valy+ply._errmax);
    CMASolutions eyx_up = cmaes(func,nparameters);
    
    // find lower x value for y parameter.
    nparameters = parameters;
    nparameters.set_fixed_p(py,valy+ply._errmin);
    CMASolutions eyx_lo = cmaes(func,nparameters);

    contour c;
    c.add_point(valx+plx._errmin,exy_lo.best_candidate().get_x()[py]);
    c.add_point(eyx_lo.best_candidate().get_x()[px],valy+ply._errmin); 
    c.add_point(valx+plx._errmax,exy_up.best_candidate().get_x()[py]);
    c.add_point(eyx_up.best_candidate().get_x()[px],valy+ply._errmax);
    
    //TODO: more than 4 points.
    
    return c;
  }
  
  template class errstats<GenoPheno<NoBoundStrategy>>;
  template class errstats<GenoPheno<pwqBoundStrategy>>;
  template class errstats<GenoPheno<NoBoundStrategy,linScalingStrategy>>;
  template class errstats<GenoPheno<pwqBoundStrategy,linScalingStrategy>>;
}