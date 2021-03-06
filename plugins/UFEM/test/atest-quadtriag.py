import sys
import coolfluid as cf

# Global configuration
cf.Core.environment().options().configure_option('log_level', 4)

# setup a model
model = cf.Core.root().create_component('HotModel', 'cf3.solver.Model')
domain = model.create_domain()
physics = model.create_physics('cf3.UFEM.NavierStokesPhysics')
solver = model.create_solver('cf3.UFEM.Solver')
hc = solver.add_direct_solver('cf3.UFEM.HeatConductionSteady')


# load the mesh (passed as first argument to the script)
mesh = domain.load_mesh(file = cf.URI(sys.argv[1]), name = 'Mesh')

# lss setup
lss = hc.create_lss('cf3.math.LSS.TrilinosFEVbrMatrix')
lss.get_child('Matrix').options().configure_option('settings_file', sys.argv[2]);

# Boundary conditions
bc = hc.get_child('BoundaryConditions')
bc.add_constant_bc(region_name = 'inlet', variable_name = 'Temperature').options().configure_option('value', 10)
bc.add_constant_bc(region_name = 'outlet', variable_name = 'Temperature').options().configure_option('value', 35)

# run the simulation
model.simulate()

# check the result
coords = mesh.access_component('geometry/coordinates')
temperature = mesh.access_component('geometry/solution')
length = 0.
for i in range(len(coords)):
  x = coords[i][0]
  if x > length:
    length = x

for i in range(len(temperature)):
  if abs(10. + 25.*(coords[i][0] / length) - temperature[i][0]) > 1e-12:
    raise Exception('Incorrect temperature ' + str(temperature[i][0]) + ' for point ' + str(coords[i][0]))

# Write result
domain.write_mesh(cf.URI('atest-quadtriag_output.pvtu'))

