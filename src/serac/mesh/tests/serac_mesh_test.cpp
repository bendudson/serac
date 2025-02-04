// Copyright (c) 2019-2022, Lawrence Livermore National Security, LLC and
// other Serac Project Developers. See the top-level LICENSE file for
// details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

#include "serac/mesh/mesh_utils.hpp"

#include <fstream>

#include <gtest/gtest.h>
#include "mfem.hpp"

#include "serac/serac_config.hpp"

namespace serac {

TEST(mesh, load_exodus)
{
  MPI_Barrier(MPI_COMM_WORLD);
  std::string mesh_file = std::string(SERAC_REPO_DIR) + "/data/meshes/bortel_echem.e";

  auto pmesh = mesh::refineAndDistribute(buildMeshFromFile(mesh_file), 1, 1);
  MPI_Barrier(MPI_COMM_WORLD);
}

}  // namespace serac

//------------------------------------------------------------------------------
#include "axom/slic/core/SimpleLogger.hpp"

int main(int argc, char* argv[])
{
  int result = 0;

  ::testing::InitGoogleTest(&argc, argv);

  MPI_Init(&argc, &argv);

  axom::slic::SimpleLogger logger;  // create & initialize test logger, finalized when exiting main scope

  result = RUN_ALL_TESTS();

  MPI_Finalize();

  return result;
}
