// Helper functions
double DegMin2DecDeg(double degrees, double minutes);
std::string TenthsMinutestoStr(int tenths_minutes);
int DecDegToTenthsMinutes(double decimal_degrees);
void report(const char *name, const char *type, std::vector<int> &vec);

struct degmin {
    double deg;
    double min;
};
