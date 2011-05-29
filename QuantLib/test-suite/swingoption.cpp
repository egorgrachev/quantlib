/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2010, 2011 Klaus Spanderen

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "swingoption.hpp"
#include "utilities.hpp"

#include <ql/math/functional.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/instruments/vanillaswingoption.hpp>
#include <ql/instruments/vanillastorageoption.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/math/statistics/generalstatistics.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <ql/processes/stochasticprocessarray.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/methods/montecarlo/multipathgenerator.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/experimental/processes/gemanroncoroniprocess.hpp>
#include <ql/experimental/processes/extouwithjumpsprocess.hpp>
#include <ql/experimental/processes/extendedornsteinuhlenbeckprocess.hpp>
#include <ql/experimental/finitedifferences/fdsimplebsswingengine.hpp>
#include <ql/experimental/finitedifferences/fdextoujumpvanillaengine.hpp>
#include <ql/experimental/finitedifferences/fdsimpleextoustorageengine.hpp>
#include <ql/experimental/finitedifferences/fdsimpleextoujumpswingengine.hpp>
#include <ql/experimental/finitedifferences/exponentialjump1dmesher.hpp>
#include <ql/experimental/finitedifferences/fdblackscholesvanillaengine.hpp>

#include <boost/lambda/lambda.hpp>
#include <deque>

using namespace QuantLib;
using namespace boost::unit_test_framework;


void SwingOptionTest::testExtendedOrnsteinUhlenbeckProcess() {

    BOOST_MESSAGE("Testing extended Ornstein-Uhlenbeck process...");

    SavedSettings backup;

	const Real speed = 2.5;
	const Volatility vol = 0.70;
	const Real level = 1.43;

	ExtendedOrnsteinUhlenbeckProcess::Discretization discr[] = {
		ExtendedOrnsteinUhlenbeckProcess::MidPoint,
		ExtendedOrnsteinUhlenbeckProcess::Trapezodial,
		ExtendedOrnsteinUhlenbeckProcess::GaussLobatto};

	boost::function<Real (Real)> f[] 
		= { constant<Real, Real>(level),
			std::bind1st(std::plus<Real>(), 1.0),
			std::ptr_fun<Real, Real>(std::sin) }; 

	for (Size n=0; n < LENGTH(f); ++n) {
		ExtendedOrnsteinUhlenbeckProcess refProcess(
			speed, vol, 0.0, f[n], 
			ExtendedOrnsteinUhlenbeckProcess::GaussLobatto, 1e-6);

		for (Size i=0; i < LENGTH(discr)-1; ++i) {
			ExtendedOrnsteinUhlenbeckProcess eouProcess(
									  speed, vol, 0.0, f[n], discr[i]);

			const Time T = 10;
			const Size nTimeSteps = 10000;

			const Time dt = T/nTimeSteps;
			Time t  = 0.0;
			Real q = 0.0;
			Real p = 0.0;
			
			PseudoRandom::rng_type rng(PseudoRandom::urng_type(1234u));

			for (Size j=0; j < nTimeSteps; ++j) {
				const Real dw = rng.next().value;
				q=eouProcess.evolve(t,q,dt,dw);
				p=refProcess.evolve(t,p,dt,dw);

				if (std::fabs(q-p) > 1e-6) {
					BOOST_FAIL("invalid process evaluation " 
					            << n << " " << i << " " << j << " " << q-p);
				}
				t+=dt;
			}
		}
	}
}


void SwingOptionTest::testGemanRoncoroniProcess() {

    BOOST_MESSAGE("Testing Geman Roncoroni process...");

    /* Example induced by H. Geman, A. Roncoroni,
       "Understanding the Fine Structure of Electricity Prices",
       http://papers.ssrn.com/sol3/papers.cfm?abstract_id=638322
       Results are verified against the authors MatLab-Code.
       http://semeq.unipmn.it/files/Ch19_spark_spread.zip
    */

    SavedSettings backup;

    const Date today = Date::todaysDate();
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = ActualActual();
    
    boost::shared_ptr<YieldTermStructure> rTS = flatRate(today, 0.03, dc);
        
	const Real x0     = 3.3;
	const Real beta   = 0.05;
    const Real alpha  = 3.1;
	const Real gamma  = -0.09;
	const Real delta  = 0.07;
	const Real eps    = -0.40;
	const Real zeta   = -0.40;
	const Real d      = 1.6;
	const Real k      = 1.0;
	const Real tau    = 0.5;
	const Real sig2   = 10.0;
	const Real a      =-7.0;
	const Real b      =-0.3;
	const Real theta1 = 35.0;
	const Real theta2 = 9.0;
	const Real theta3 = 0.10;
	const Real psi    = 1.9;

    boost::shared_ptr<GemanRoncoroniProcess> grProcess(
                new GemanRoncoroniProcess(x0, alpha, beta, gamma, delta,
        								  eps, zeta, d, k, tau, sig2, a, b, 
        								  theta1, theta2, theta3, psi));

    const Real speed     = 5.0;
    const Volatility vol = std::sqrt(1.4);    
    const Real betaG     = 0.08;
    const Real alphaG    = 1.0;
    const Real x0G       = 1.1;

    boost::function<Real (Real)> f = alphaG + betaG*boost::lambda::_1;

    boost::shared_ptr<StochasticProcess1D> eouProcess(
        new ExtendedOrnsteinUhlenbeckProcess(speed, vol, x0G, f,
                               ExtendedOrnsteinUhlenbeckProcess::Trapezodial));

    std::vector<boost::shared_ptr<StochasticProcess1D> > processes;
    processes.push_back(grProcess);
    processes.push_back(eouProcess);
    
    Matrix correlation(2, 2, 1.0);
    correlation[0][1] = correlation[1][0] = 0.25;
    
    boost::shared_ptr<StochasticProcess> pArray(
                           new StochasticProcessArray(processes, correlation));
    
    const Time T = 10.0;
    const Size stepsPerYear = 250;
    const Size steps = T*stepsPerYear;
    
    TimeGrid grid(T, steps);

    typedef PseudoRandom::rsg_type rsg_type;
    typedef MultiPathGenerator<rsg_type>::sample_type sample_type;
    rsg_type rsg = PseudoRandom::make_sequence_generator(
                               pArray->size()*(grid.size()-1), BigNatural(421));

    GeneralStatistics npv, onTime;    
    MultiPathGenerator<rsg_type> generator(pArray, grid, rsg, false);
        
    const Real heatRate = 8.0;
    const Size nrTrails = 250;
    
    for (Size n=0; n < nrTrails; ++n) {
        Real plantValue = 0.0;
        sample_type path = generator.next();

        for (Size i=1; i <= steps; ++i) {
            const Time t = Real(i)/stepsPerYear;
            const DiscountFactor df = rTS->discount(t);
            
            const Real gasPrice         = std::exp(path.value[1][i]);
            const Real electricityPrice = std::exp(path.value[0][i]);
            
            const Real sparkSpread = electricityPrice - heatRate*gasPrice;
            plantValue += std::max(0.0, sparkSpread)*df;
            onTime.add((sparkSpread > 0.0) ? 1.0 : 0.0);            
        }
                
        npv.add(plantValue);
    }
    
    const Real expectedNPV = 12500;
    const Real calculatedNPV = npv.mean();
    const Real errorEstimateNPV = npv.errorEstimate();
    
    if (std::fabs(calculatedNPV - expectedNPV) > 3.0*errorEstimateNPV) {
        BOOST_ERROR("Failed to reproduce cached price with MC engine"
                    << "\n    calculated: " << calculatedNPV
                    << "\n    expected:   " << expectedNPV
                    << " +/- " << errorEstimateNPV);
    }
    
    const Real expectedOnTime = 0.43;
    const Real calculatedOnTime = onTime.mean();
    const Real errorEstimateOnTime 
        = std::sqrt(calculatedOnTime*(1-calculatedOnTime))/nrTrails;
    
    if (std::fabs(calculatedOnTime - expectedOnTime)>3.0*errorEstimateOnTime) {
        BOOST_ERROR("Failed to reproduce cached price with MC engine"
                    << "\n    calculated: " << calculatedNPV
                    << "\n    expected:   " << expectedNPV
                    << " +/- " << errorEstimateNPV);
    }
}

void SwingOptionTest::testFdmExponentialJump1dMesher() {

    BOOST_MESSAGE("Testing finite difference mesher for the Kluge model ...");

    Array x(2, 1.0);
    const Real beta = 100.0;
    const Real eta  = 1.0/0.4;
    const Real jumpIntensity = 4.0;
    const Size dummySteps  = 2;

    ExponentialJump1dMesher mesher(dummySteps, beta, jumpIntensity, eta);

    boost::shared_ptr<ExtendedOrnsteinUhlenbeckProcess> ouProcess(
        new ExtendedOrnsteinUhlenbeckProcess(1.0, 1.0, x[0],
                                             constant<Real, Real>(1.0)));
    boost::shared_ptr<ExtOUWithJumpsProcess> jumpProcess(
        new ExtOUWithJumpsProcess(ouProcess, x[1], beta, jumpIntensity, eta));

    const Time dt = 1.0/(10.0*beta);
    const Size n = 1000000;

    std::vector<Real> path(n);
    PseudoRandom::rng_type mt(PseudoRandom::urng_type(123));
    Array dw(3);
    for (Size i=0; i < n; ++i) {
        dw[0] = mt.next().value;
        dw[1] = mt.next().value;
        dw[2] = mt.next().value;
        path[i] = (x = jumpProcess->evolve(0.0, x, dt, dw))[1];
    }
    std::sort(path.begin(), path.end());

    const Real relTol1 = 2e-3;
    const Real relTol2 = 2e-2;
    const Real threshold = 0.9;

    for (Real x=1e-12; x < 1.0; x*=10) {
        const Real v = mesher.jumpSizeDistribution(x);

        std::vector<Real>::iterator iter
            = std::lower_bound(path.begin(), path.end(), x);
        const Real q = std::distance(path.begin(), iter)/Real(n);
        QL_REQUIRE(std::fabs(q - v) < relTol1
                    || (v < threshold) && std::fabs(q-v) < relTol2,
                    "can not reproduce jump distribution");
    }
}

void SwingOptionTest::testExtOUJumpVanillaEngine() {

    BOOST_MESSAGE("Testing finite difference pricer for the Kluge model ...");

    Array x0(2);
    x0[0] = 3.0; x0[1] = 0.0;

    const Real beta = 5.0;
    const Real eta  = 2.0;
    const Real jumpIntensity = 1.0;
    const Real speed = 1.0;
    const Real volatility = 2.0;
    const Rate irRate = 0.10;

    boost::shared_ptr<ExtendedOrnsteinUhlenbeckProcess> ouProcess(
        new ExtendedOrnsteinUhlenbeckProcess(speed, volatility, x0[0],
                                             constant<Real, Real>(x0[0])));
    boost::shared_ptr<ExtOUWithJumpsProcess> jumpProcess(
        new ExtOUWithJumpsProcess(ouProcess, x0[1], beta, jumpIntensity, eta));


    const Date today = Date::todaysDate();
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = ActualActual();
    const Date maturityDate = today + Period(12, Months);
    const Time maturity = dc.yearFraction(today, maturityDate);

    boost::shared_ptr<YieldTermStructure> rTS(flatRate(today, irRate, dc));
    boost::shared_ptr<StrikedTypePayoff> payoff(
                                     new PlainVanillaPayoff(Option::Call, 30));
    boost::shared_ptr<Exercise> exercise(new EuropeanExercise(maturityDate));

    boost::shared_ptr<PricingEngine> engine(
                 new FdExtOUJumpVanillaEngine(jumpProcess, rTS, 25, 200, 50));

    VanillaOption option(payoff, exercise);
    option.setPricingEngine(engine);
    const Real fdNPV = option.NPV();

    const Size steps = 100;
    const Size nrTrails = 200000;
    TimeGrid grid(maturity, steps);

    typedef PseudoRandom::rsg_type rsg_type;
    typedef MultiPathGenerator<rsg_type>::sample_type sample_type;
    rsg_type rsg = PseudoRandom::make_sequence_generator(
                    jumpProcess->factors()*(grid.size()-1), BigNatural(421));

    GeneralStatistics npv;
    MultiPathGenerator<rsg_type> generator(jumpProcess, grid, rsg, false);

    for (Size n=0; n < nrTrails; ++n) {
        sample_type path = generator.next();

        const Real x = path.value[0].back();
        const Real y = path.value[1].back();

        const Real cashflow = (*payoff)(std::exp(x+y));
        npv.add(cashflow*rTS->discount(maturity));
    }

    const Real mcNPV = npv.mean();
    const Real mcError = npv.errorEstimate();

    if ( std::fabs(fdNPV - mcNPV) > 3.0*mcError) {
        BOOST_ERROR("Failed to reproduce FD and MC prices"
                    << "\n    FD NPV: " << fdNPV
                    << "\n    MC NPV: " << mcNPV
                    << " +/- " << mcError);
    }
}

void SwingOptionTest::testFdBSSwingOption() {

    BOOST_MESSAGE("Testing Black-Scholes Vanilla Swing option pricing ...");

    Date settlementDate = Date::todaysDate();
    Settings::instance().evaluationDate() = settlementDate;
    DayCounter dayCounter = ActualActual();
    Date maturityDate = settlementDate + Period(12, Months);

    boost::shared_ptr<StrikedTypePayoff> payoff(
                                     new PlainVanillaPayoff(Option::Put, 30));

    std::vector<Date> exerciseDates(1, settlementDate+Period(1, Months));
    while (exerciseDates.back() < maturityDate) {
        exerciseDates.push_back(exerciseDates.back()+Period(1, Months));
    }
    boost::shared_ptr<BermudanExercise> bermudanExercise(
                                        new BermudanExercise(exerciseDates));

    Handle<YieldTermStructure> riskFreeTS(flatRate(0.14, dayCounter));
    Handle<YieldTermStructure> dividendTS(flatRate(0.02, dayCounter));
    Handle<BlackVolTermStructure> volTS(
                                    flatVol(settlementDate, 0.4, dayCounter));

    Handle<Quote> s0(boost::shared_ptr<Quote>(new SimpleQuote(30.0)));

    boost::shared_ptr<BlackScholesMertonProcess> process(
            new BlackScholesMertonProcess(s0, dividendTS, riskFreeTS, volTS));
    boost::shared_ptr<PricingEngine> engine(
                                new FdSimpleBSSwingEngine(process, 50, 200));
    
    VanillaOption bermudanOption(payoff, bermudanExercise);
    bermudanOption.setPricingEngine(boost::shared_ptr<PricingEngine>(
                          new FdBlackScholesVanillaEngine(process, 50, 200)));
    const Real bermudanOptionPrices = bermudanOption.NPV();
    
    for (Size i=0; i < exerciseDates.size(); ++i) {
        const Size exerciseRights = i+1;
        
        VanillaSwingOption swingOption(payoff, bermudanExercise,
                                       exerciseRights, exerciseRights);
        swingOption.setPricingEngine(engine);
        const Real swingOptionPrice = swingOption.NPV();

        const Real upperBound = exerciseRights*bermudanOptionPrices;

        if (swingOptionPrice - upperBound > 2e-2) {
            BOOST_ERROR("Failed to reproduce upper bounds"
                        << "\n    upper Bound: " << upperBound
                        << "\n    Price:       " << swingOptionPrice);
        }
        
        Real lowerBound = 0.0;
        for (Size j=exerciseDates.size()-i-1; j < exerciseDates.size(); ++j) {
            VanillaOption europeanOption(payoff, boost::shared_ptr<Exercise>(
                                     new EuropeanExercise(exerciseDates[j])));
            europeanOption.setPricingEngine(
                boost::shared_ptr<PricingEngine>(
                                          new AnalyticEuropeanEngine(process)));
            lowerBound += europeanOption.NPV();
        }

        if (lowerBound - swingOptionPrice > 2e-2) {
            BOOST_ERROR("Failed to reproduce lower bounds"
                        << "\n    lower Bound: " << lowerBound
                        << "\n    Price:       " << swingOptionPrice);
        }
    }
}


void SwingOptionTest::testExtOUJumpSwingOption() {

    BOOST_MESSAGE("Testing Simple Swing option pricing for Kluge model...");

    Date settlementDate = Date::todaysDate();
    Settings::instance().evaluationDate() = settlementDate;
    DayCounter dayCounter = ActualActual();
    Date maturityDate = settlementDate + Period(12, Months);

    boost::shared_ptr<StrikedTypePayoff> payoff(
                                     new PlainVanillaPayoff(Option::Put, 30));

    std::vector<Date> exerciseDates(1, settlementDate+Period(1, Months));
    while (exerciseDates.back() < maturityDate) {
        exerciseDates.push_back(exerciseDates.back()+Period(1, Months));
    }
    boost::shared_ptr<BermudanExercise> bermudanExercise(
                                        new BermudanExercise(exerciseDates));

    std::vector<Time> exerciseTimes(exerciseDates.size());
    for (Size i=0; i < exerciseTimes.size(); ++i) {
        exerciseTimes[i]
                 = dayCounter.yearFraction(settlementDate, exerciseDates[i]);
    }

    TimeGrid grid(exerciseTimes.begin(), exerciseTimes.end(), 60);
    std::vector<Size> exerciseIndex(exerciseDates.size());
    for (Size i=0; i < exerciseIndex.size(); ++i) {
        exerciseIndex[i] = grid.closestIndex(exerciseTimes[i]);
    }

    Array x0(2);
    x0[0] = 3.0; x0[1] = 0.0;

    const Real beta = 5.0;
    const Real eta  = 2.0;
    const Real jumpIntensity = 1.0;
    const Real speed = 1.0;
    const Real volatility = 2.0;
    const Rate irRate = 0.1;

    boost::shared_ptr<ExtendedOrnsteinUhlenbeckProcess> ouProcess(
        new ExtendedOrnsteinUhlenbeckProcess(speed, volatility, x0[0],
                                             constant<Real, Real>(x0[0])));
    boost::shared_ptr<ExtOUWithJumpsProcess> jumpProcess(
        new ExtOUWithJumpsProcess(ouProcess, x0[1], beta, jumpIntensity, eta));

    boost::shared_ptr<YieldTermStructure> rTS(
                                flatRate(settlementDate, irRate, dayCounter));

    boost::shared_ptr<PricingEngine> swingEngine(
                new FdSimpleExtOUJumpSwingEngine(jumpProcess, rTS, 25, 50, 25));

    boost::shared_ptr<PricingEngine> vanillaEngine(
                new FdExtOUJumpVanillaEngine(jumpProcess, rTS, 25, 50, 25));

    VanillaOption bermudanOption(payoff, bermudanExercise);
    bermudanOption.setPricingEngine(vanillaEngine);
    const Real bermudanOptionPrices = bermudanOption.NPV();

    const Size nrTrails = 16000;
    typedef PseudoRandom::rsg_type rsg_type;
    typedef MultiPathGenerator<rsg_type>::sample_type sample_type;
    rsg_type rsg = PseudoRandom::make_sequence_generator(
                    jumpProcess->factors()*(grid.size()-1), BigNatural(421));

    MultiPathGenerator<rsg_type> generator(jumpProcess, grid, rsg, false);

    for (Size i=0; i < exerciseDates.size(); ++i) {
        const Size exerciseRights = i+1;

        VanillaSwingOption swingOption(payoff, bermudanExercise,
                                       exerciseRights, exerciseRights);
        swingOption.setPricingEngine(swingEngine);
        const Real swingOptionPrice = swingOption.NPV();

        const Real upperBound = exerciseRights*bermudanOptionPrices;

        if (swingOptionPrice - upperBound > 2e-2) {
            BOOST_ERROR("Failed to reproduce upper bounds"
                        << "\n    upper Bound: " << upperBound
                        << "\n    Price:       " << swingOptionPrice);
        }

        Real lowerBound = 0.0;
        for (Size j=exerciseDates.size()-i-1; j < exerciseDates.size(); ++j) {
            VanillaOption europeanOption(payoff, boost::shared_ptr<Exercise>(
                                     new EuropeanExercise(exerciseDates[j])));
            europeanOption.setPricingEngine(
                boost::shared_ptr<PricingEngine>(vanillaEngine));
            lowerBound += europeanOption.NPV();
        }

        if (lowerBound - swingOptionPrice > 2e-2) {
            BOOST_ERROR("Failed to reproduce lower bounds"
                       << "\n    lower Bound: " << lowerBound
                       << "\n    Price:       " << swingOptionPrice);
        }

        // use MC plus perfect forecast to find an upper bound
        GeneralStatistics npv;
        for (Size n=0; n < nrTrails; ++n) {
            sample_type path = generator.next();

            std::vector<Real> exerciseValues(exerciseTimes.size());
            for (Size k=0; k < exerciseTimes.size(); ++k) {
                const Real x = path.value[0][exerciseIndex[k]];
                const Real y = path.value[1][exerciseIndex[k]];
                const Real s = std::exp(x+y);

                exerciseValues[k] =(*payoff)(s)*rTS->discount(exerciseDates[k]);
            }
            std::sort(exerciseValues.begin(), exerciseValues.end(),
                      std::greater<Real>());

            Real npCashFlows
                = std::accumulate(exerciseValues.begin(),
                                  exerciseValues.begin()+exerciseRights, 0.0);
            npv.add(npCashFlows);
        }

        const Real mcUpperBound = npv.mean();
        const Real mcErrorUpperBound = npv.errorEstimate();
        if (swingOptionPrice - mcUpperBound > 2.36*mcErrorUpperBound) {
            BOOST_ERROR("Failed to reproduce mc upper bounds"
                       << "\n    mc upper Bound: " << mcUpperBound
                       << "\n    Price:          " << swingOptionPrice);
        }
    }
}

void SwingOptionTest::testSimpleExtOUStorageEngine() {

    BOOST_MESSAGE("Testing Simple Storage option based on ext. OU  model...");

    Date settlementDate = Date::todaysDate();
    Settings::instance().evaluationDate() = settlementDate;
    DayCounter dayCounter = ActualActual();
    Date maturityDate = settlementDate + Period(12, Months);

    std::vector<Date> exerciseDates(1, settlementDate+Period(1, Days));
    while (exerciseDates.back() < maturityDate) {
        exerciseDates.push_back(exerciseDates.back()+Period(1, Days));
    }
    boost::shared_ptr<BermudanExercise> bermudanExercise(
                                        new BermudanExercise(exerciseDates));

    const Real x0 = 3.0;
    const Real speed = 1.0;
    const Real volatility = 0.5;
    const Rate irRate = 0.1;

    boost::shared_ptr<ExtendedOrnsteinUhlenbeckProcess> ouProcess(
        new ExtendedOrnsteinUhlenbeckProcess(speed, volatility, x0,
                                             constant<Real, Real>(x0)));

    boost::shared_ptr<YieldTermStructure> rTS(
                                flatRate(settlementDate, irRate, dayCounter));

    boost::shared_ptr<PricingEngine> storageEngine(
               new FdSimpleExtOUStorageEngine(ouProcess, rTS, 1, 25));

    VanillaStorageOption storageOption(bermudanExercise, 50, 0, 1);

    storageOption.setPricingEngine(storageEngine);

    const Real expected = 69.6914;
    const Real calculated = storageOption.NPV();

    if (std::fabs(expected - calculated) > 2e-2) {
        BOOST_ERROR("Failed to reproduce cached values"
                   << "\n    calculated: " << calculated
                   << "\n    expected:   " << expected);
    }
}


test_suite* SwingOptionTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("Spark-Option Test");
    suite->add(QUANTLIB_TEST_CASE(
		&SwingOptionTest::testExtendedOrnsteinUhlenbeckProcess));
	suite->add(QUANTLIB_TEST_CASE(
		&SwingOptionTest::testGemanRoncoroniProcess));
    suite->add(QUANTLIB_TEST_CASE(&SwingOptionTest::testFdBSSwingOption));
    suite->add(QUANTLIB_TEST_CASE(
                          &SwingOptionTest::testFdmExponentialJump1dMesher));

    suite->add(QUANTLIB_TEST_CASE(&SwingOptionTest::testExtOUJumpVanillaEngine));
    suite->add(QUANTLIB_TEST_CASE(
                            &SwingOptionTest::testExtOUJumpSwingOption));
    suite->add(QUANTLIB_TEST_CASE(
                            &SwingOptionTest::testSimpleExtOUStorageEngine));
    
    return suite;
}
