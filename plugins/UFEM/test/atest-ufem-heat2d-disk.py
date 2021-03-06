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
domain.load_mesh(file = cf.URI(sys.argv[1]), name = 'Mesh')

# lss setup
lss = hc.create_lss('cf3.math.LSS.TrilinosFEVbrMatrix')
lss.get_child('Matrix').options().configure_option('settings_file', sys.argv[2]);

# Boundary conditions
bc = hc.get_child('BoundaryConditions')
bc.add_constant_bc(region_name = 'inner', variable_name = 'Temperature').options().configure_option('value', 10)
bc.add_constant_bc(region_name = 'outer', variable_name = 'Temperature').options().configure_option('value', 35)

# run the simulation
model.simulate()

# Write result
domain.write_mesh(cf.URI('atest-ufem-heat2d-disk.pvtu'))
