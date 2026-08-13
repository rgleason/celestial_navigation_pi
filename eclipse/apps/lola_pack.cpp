#include <netcdf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

void Check(int status, const std::string& operation) {
  if (status != NC_NOERR) {
    std::cerr << operation << ": " << nc_strerror(status) << '\n';
    std::exit(1);
  }
}

int FindVariable(int file, const char* first, const char* second) {
  int id = -1;
  if (nc_inq_varid(file, first, &id) == NC_NOERR) return id;
  if (second && nc_inq_varid(file, second, &id) == NC_NOERR) return id;
  return -1;
}

std::size_t Closest(const std::vector<double>& values, double target,
                    bool longitude) {
  std::size_t best = 0;
  double best_error = std::numeric_limits<double>::infinity();
  for (std::size_t index = 0; index < values.size(); ++index) {
    double error = std::fabs(values[index] - target);
    if (longitude) error = std::min(error, std::fabs(error - 360.0));
    if (error < best_error) {
      best_error = error;
      best = index;
    }
  }
  return best;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: lola-pack <LDEM64_PA_pixel.grd> <lola64-pa.bin>\n";
    return 2;
  }
  int file = -1;
  Check(nc_open(argv[1], NC_NOWRITE, &file), "opening netCDF grid");
  const int z_id = FindVariable(file, "z", "elevation");
  const int x_id = FindVariable(file, "x", "lon");
  const int y_id = FindVariable(file, "y", "lat");
  if (z_id < 0 || x_id < 0 || y_id < 0) {
    std::cerr
        << "LOLA grid must contain z/elevation, x/lon and y/lat variables\n";
    nc_close(file);
    return 1;
  }
  int dimensions = 0;
  int dimension_ids[NC_MAX_VAR_DIMS];
  Check(nc_inq_var(file, z_id, NULL, NULL, &dimensions, dimension_ids, NULL),
        "reading elevation dimensions");
  if (dimensions != 2) {
    std::cerr << "LOLA elevation variable is not a two-dimensional grid\n";
    nc_close(file);
    return 1;
  }
  std::size_t first_length = 0, second_length = 0;
  Check(nc_inq_dimlen(file, dimension_ids[0], &first_length),
        "reading first grid dimension");
  Check(nc_inq_dimlen(file, dimension_ids[1], &second_length),
        "reading second grid dimension");
  if (second_length != first_length * 2) {
    std::cerr << "Expected a row-major 2:1 global pixel grid, found "
              << first_length << 'x' << second_length << '\n';
    nc_close(file);
    return 1;
  }
  const int height = static_cast<int>(first_length);
  const int width = static_cast<int>(second_length);
  std::vector<double> longitudes(static_cast<std::size_t>(width));
  std::vector<double> latitudes(static_cast<std::size_t>(height));
  Check(nc_get_var_double(file, x_id, &longitudes[0]), "reading longitudes");
  Check(nc_get_var_double(file, y_id, &latitudes[0]), "reading latitudes");

  std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
  if (!output) {
    std::cerr << "Unable to create " << argv[2] << '\n';
    nc_close(file);
    return 1;
  }
  const char magic[8] = {'C', 'L', 'R', 'O', '6', '4', '\r', '\n'};
  const std::uint32_t output_width = static_cast<std::uint32_t>(width);
  const std::uint32_t output_height = static_cast<std::uint32_t>(height);
  const double reference_radius_km = 1737.4;
  output.write(magic, 8);
  output.write(reinterpret_cast<const char*>(&output_width), 4);
  output.write(reinterpret_cast<const char*>(&output_height), 4);
  output.write(reinterpret_cast<const char*>(&reference_radius_km), 8);

  std::vector<float> source_row(static_cast<std::size_t>(width));
  std::vector<std::int16_t> destination_row(static_cast<std::size_t>(width));
  std::vector<std::size_t> longitude_index(static_cast<std::size_t>(width));
  for (int column = 0; column < width; ++column) {
    const double longitude = 360.0 * (column + 0.5) / width;
    longitude_index[column] = Closest(longitudes, longitude, true);
  }
  for (int output_row = 0; output_row < height; ++output_row) {
    const double latitude = 90.0 - 180.0 * (output_row + 0.5) / height;
    const std::size_t source_row_index = Closest(latitudes, latitude, false);
    const std::size_t start[2] = {source_row_index, 0};
    const std::size_t count[2] = {1, static_cast<std::size_t>(width)};
    Check(nc_get_vara_float(file, z_id, start, count, &source_row[0]),
          "reading elevation row");
    for (int column = 0; column < width; ++column) {
      const float value = source_row[longitude_index[column]];
      double offset_metres = value;
      // NASA's products have appeared both as elevation in metres and as
      // radius in kilometres. Detect the latter without relying on a mutable
      // metadata spelling.
      if (value > 1600.0f && value < 1900.0f)
        offset_metres = (value - reference_radius_km) * 1000.0;
      else if (std::fabs(value) < 30.0f)
        offset_metres = value * 1000.0;
      if (!std::isfinite(offset_metres) || offset_metres < -32767.0 ||
          offset_metres > 32767.0) {
        destination_row[column] = std::numeric_limits<std::int16_t>::min();
      } else {
        destination_row[column] =
            static_cast<std::int16_t>(std::lround(offset_metres));
      }
    }
    output.write(reinterpret_cast<const char*>(&destination_row[0]),
                 static_cast<std::streamsize>(destination_row.size() * 2));
    if (!output) {
      std::cerr << "Error writing LOLA limb pack\n";
      nc_close(file);
      return 1;
    }
    if (output_row % 512 == 0)
      std::cerr << "Converted row " << output_row << " of " << height << '\n';
  }
  Check(nc_close(file), "closing netCDF grid");
  std::cout << "Created " << argv[2] << " (" << width << 'x' << height
            << ", int16 metre offsets, MOON_PA)\n";
  return 0;
}
