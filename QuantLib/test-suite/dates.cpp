/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004 Ferdinando Ametrano
 Copyright (C) 2006 Katiuscia Manzoni
 Copyright (C) 2003 RiskMap srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/reference/license.html>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "dates.hpp"
#include <ql/time/date.hpp>
#include <ql/utilities/dataparsers.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

void DateTest::immDates() {
    BOOST_MESSAGE("Testing IMM dates...");

    const std::string IMMcodes[] = {
        "F0", "G0", "H0", "J0", "K0", "M0", "N0", "Q0", "U0", "V0", "X0", "Z0",
        "F1", "G1", "H1", "J1", "K1", "M1", "N1", "Q1", "U1", "V1", "X1", "Z1",
        "F2", "G2", "H2", "J2", "K2", "M2", "N2", "Q2", "U2", "V2", "X2", "Z2",
        "F3", "G3", "H3", "J3", "K3", "M3", "N3", "Q3", "U3", "V3", "X3", "Z3",
        "F4", "G4", "H4", "J4", "K4", "M4", "N4", "Q4", "U4", "V4", "X4", "Z4",
        "F5", "G5", "H5", "J5", "K5", "M5", "N5", "Q5", "U5", "V5", "X5", "Z5",
        "F6", "G6", "H6", "J6", "K6", "M6", "N6", "Q6", "U6", "V6", "X6", "Z6",
        "F7", "G7", "H7", "J7", "K7", "M7", "N7", "Q7", "U7", "V7", "X7", "Z7",
        "F8", "G8", "H8", "J8", "K8", "M8", "N8", "Q8", "U8", "V8", "X8", "Z8",
        "F9", "G9", "H9", "J9", "K9", "M9", "N9", "Q9", "U9", "V9", "X9", "Z9"
    };

    Date counter = Date::minDate();
    // 10 years of futures must not exceed Date::maxDate
    Date last = Date::maxDate() - 121*Months;
    Date imm;

    while (counter<=last) {
        imm = Date::nextIMMdate(counter, false);

        // check that imm is greater than counter
        if (imm<=counter)
            BOOST_FAIL("\n  "
                       << imm.weekday() << " " << imm
                       << " is not greater than "
                       << counter.weekday() << " " << counter);

        // check that imm is an IMM date
        if (!Date::isIMMdate(imm, false))
            BOOST_FAIL("\n  "
                       << imm.weekday() << " " << imm
                       << " is not an IMM date (calculated from "
                       << counter.weekday() << " " << counter << ")");

        // check that imm is <= to the next IMM date in the main cycle
        if (imm>Date::nextIMMdate(counter, true))
            BOOST_FAIL("\n  "
                       << imm.weekday() << " " << imm
                       << " is not less than or equal to the next future in the main cycle "
                       << Date::nextIMMdate(counter, true));

        //// check that if counter is an IMM date, then imm==counter
        //if (Date::isIMMdate(counter, false) && (imm!=counter))
        //    BOOST_FAIL("\n  "
        //               << counter.weekday() << " " << counter
        //               << " is already an IMM date, while nextIMM() returns "
        //               << imm.weekday() << " " << imm);

        // check that for every date IMMdate is the inverse of IMMcode
        if (Date::IMMdate(Date::IMMcode(imm), counter) != imm)
            BOOST_FAIL("\n  "
                       << Date::IMMcode(imm)
                       << " at calendar day " << counter
                       << " is not the IMM code matching " << imm);

        // check that for every date the 120 IMM codes refer to future dates
        for (int i=0; i<40; ++i) {
            if (Date::IMMdate(IMMcodes[i], counter)<counter)
                BOOST_FAIL("\n  "
                       << Date::IMMdate(IMMcodes[i], counter)
                       << " is wrong for " << IMMcodes[i]
                       << " at reference date " << counter);
        }

        counter = counter + 1;
    }
}

void DateTest::testConsistency() {

    BOOST_MESSAGE("Testing dates...");

    BigInteger minDate = Date::minDate().serialNumber()+1,
               maxDate = Date::maxDate().serialNumber();

    BigInteger dyold = Date(minDate-1).dayOfYear(),
               dold  = Date(minDate-1).dayOfMonth(),
               mold  = Date(minDate-1).month(),
               yold  = Date(minDate-1).year(),
               wdold = Date(minDate-1).weekday();

    for (BigInteger i=minDate; i<=maxDate; i++) {
        Date t(i);
        BigInteger serial = t.serialNumber();

        // check serial number consistency
        if (serial != i)
            BOOST_FAIL("inconsistent serial number:\n"
                       << "    original:      " << i << "\n"
                       << "    date:          " << t << "\n"
                       << "    serial number: " << serial);

        Integer dy = t.dayOfYear(),
                d  = t.dayOfMonth(),
                m  = t.month(),
                y  = t.year(),
                wd = t.weekday();

        // check if skipping any date
        if (!((dy == dyold+1) ||
              (dy == 1 && dyold == 365 && !Date::isLeap(yold)) ||
              (dy == 1 && dyold == 366 && Date::isLeap(yold))))
            BOOST_FAIL("wrong day of year increment: \n"
                       << "    date: " << t << "\n"
                       << "    day of year: " << dy << "\n"
                       << "    previous:    " << dyold);
        dyold = dy;

        if (!((d == dold+1 && m == mold   && y == yold) ||
              (d == 1      && m == mold+1 && y == yold) ||
              (d == 1      && m == 1      && y == yold+1)))
            BOOST_FAIL("wrong day,month,year increment: \n"
                       << "    date: " << t << "\n"
                       << "    day,month,year: "
                       << d << "," << Integer(m) << "," << y << "\n"
                       << "    previous:       "
                       << dold<< "," << Integer(mold) << "," << yold);
        dold = d; mold = m; yold = y;

        // check month definition
        if (m < 1 || m > 12)
            BOOST_FAIL("invalid month: \n"
                       << "    date:  " << t << "\n"
                       << "    month: " << Integer(m));

        // check day definition
        if (d < 1)
            BOOST_FAIL("invalid day of month: \n"
                       << "    date:  " << t << "\n"
                       << "    day: " << d);
        if (!((m == 1  && d <= 31) ||
              (m == 2  && d <= 28) ||
              (m == 2  && d == 29 && Date::isLeap(y)) ||
              (m == 3  && d <= 31) ||
              (m == 4  && d <= 30) ||
              (m == 5  && d <= 31) ||
              (m == 6  && d <= 30) ||
              (m == 7  && d <= 31) ||
              (m == 8  && d <= 31) ||
              (m == 9  && d <= 30) ||
              (m == 10 && d <= 31) ||
              (m == 11 && d <= 30) ||
              (m == 12 && d <= 31)))
            BOOST_FAIL("invalid day of month: \n"
                       << "    date:  " << t << "\n"
                       << "    day: " << d);

        // check weekday definition
        if (!((Integer(wd) == Integer(wdold+1)) ||
              (Integer(wd) == 1 && Integer(wdold) == 7)))
            BOOST_FAIL("invalid weekday: \n"
                       << "    date:  " << t << "\n"
                       << "    weekday:  " << Integer(wd) << "\n"
                       << "    previous: " << Integer(wdold));
        wdold = wd;

        // create the same date with a different constructor
        Date s(d,Month(m),y);
        // check serial number consistency
        serial = s.serialNumber();
        if (serial != i)
            BOOST_FAIL("inconsistent serial number:\n"
                       << "    date:          " << t << "\n"
                       << "    serial number: " << i << "\n"
                       << "    cloned date:   " <<  s << "\n"
                       << "    serial number: " << serial);
    }

}

void DateTest::isoDates() {
    BOOST_MESSAGE("Testing ISO dates...");
    std::string input_date("2006-01-15");
    Date d = DateParser::parseISO(input_date);
    if (d.dayOfMonth() != 15 ||
        d.month() != January ||
        d.year() != 2006) {
        BOOST_FAIL("Iso date failed\n"
                   << " input date:    " << input_date << "\n"
                   << " day of month:  " << d.dayOfMonth() << "\n"
                   << " month:         " << d.month() << "\n"
                   << " year:          " << d.year());
    }
}

test_suite* DateTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("Date tests");
    suite->add(BOOST_TEST_CASE(&DateTest::testConsistency));
    suite->add(BOOST_TEST_CASE(&DateTest::immDates));
    suite->add(BOOST_TEST_CASE(&DateTest::isoDates));
    return suite;
}

