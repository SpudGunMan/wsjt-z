#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H
#include <QString>

bool isValidRegexPattern(const QString& pattern);
double tx_duration(QString mode, double trPeriod, int nsps, bool bFast9);

#endif // HELPER_FUNCTIONS_H
