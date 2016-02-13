#ifndef SPECIESFACTORY_H
#define SPECIESFACTORY_H

#include "Species.h"
#include "Species_norm.h"
#include "Species_rrll.h"

#include "PusherFactory.h"
#include "IonizationFactory.h"
#include "PartBoundCond.h"

#include "Params.h"
#include "SmileiMPI.h"

#include "Tools.h"

class SpeciesFactory {
public:

    static std::vector<Species*> createVector(Params& params, SmileiMPI* smpi) {
        // this will be returned
        std::vector<Species*> retSpecies;
        
        
        // read from python namelist
        unsigned int tot_species_number = PyTools::nComponents("Species");
        for (unsigned int ispec = 0; ispec < tot_species_number; ispec++) {
            
            std::string species_type("");
            PyTools::extract("species_type",species_type,"Species",ispec);
            if(species_type.empty()) {
                std::ostringstream name("");
                name << "species" << std::setfill('0') << std::setw(log10(tot_species_number)+1) << ispec;
                species_type=name.str();
                MESSAGE("For species #" << ispec << ", parameter species_type will be " << species_type);
            }
            
            // Extract type of species dynamics from namelist
            std::string dynamics_type = "norm"; // default value
            if (!PyTools::extract("dynamics_type", dynamics_type ,"Species",ispec) )
                WARNING("For species '" << species_type << "' dynamics_type not defined: assumed = 'norm'.");
            
            // Create species object
            Species * thisSpecies=NULL;
            if (dynamics_type=="norm") {
                // Species with Boris dynamics
                thisSpecies = new Species_norm(params, smpi);
            } else if (dynamics_type=="rrll") {
                // Species with Boris dynamics + Radiation Back-Reaction (using the Landau-Lifshitz formula)
                thisSpecies = new Species_rrll(params, smpi);
            } else {
                ERROR("For species #" << ispec << ", dynamics_type must be either 'norm' or 'rrll'")
            }
            
            thisSpecies->species_type = species_type;
            thisSpecies->dynamics_type = dynamics_type;
            thisSpecies->speciesNumber = ispec;
            
            // Extract various parameters from the namelist
            
            
            PyTools::extract("initPosition_type",thisSpecies->initPosition_type ,"Species",ispec);
            if (thisSpecies->initPosition_type.empty()) {
                ERROR("For species '" << species_type << "' empty initPosition_type");
            } else if ( (thisSpecies->initPosition_type!="regular")&&(thisSpecies->initPosition_type!="random") ) {
                ERROR("For species '" << species_type << "' bad definition of initPosition_type " << thisSpecies->initPosition_type);
            }
            
            PyTools::extract("initMomentum_type",thisSpecies->initMomentum_type ,"Species",ispec);
            if ( (thisSpecies->initMomentum_type=="mj") || (thisSpecies->initMomentum_type=="maxj") ) {
                thisSpecies->initMomentum_type="maxwell-juettner";
            }
            if (   (thisSpecies->initMomentum_type!="cold")
                && (thisSpecies->initMomentum_type!="maxwell-juettner")
                && (thisSpecies->initMomentum_type!="rectangular") ) {
                ERROR("For species '" << species_type << "' bad definition of initMomentum_type");
            }
            
            PyTools::extract("c_part_max",thisSpecies->c_part_max,"Species",ispec);
            
            if( !PyTools::extract("mass",thisSpecies->mass ,"Species",ispec) ) {
                ERROR("For species '" << species_type << "' mass not defined.");
            }
            
            PyTools::extract("time_frozen",thisSpecies->time_frozen ,"Species",ispec);
            if (thisSpecies->time_frozen > 0 && thisSpecies->initMomentum_type!="cold") {
                WARNING("For species '" << species_type << "' possible conflict between time-frozen & not cold initialization");
            }
            
            PyTools::extract("radiating",thisSpecies->radiating ,"Species",ispec);
            if (thisSpecies->dynamics_type=="rrll" && (!thisSpecies->radiating)) {
                WARNING("For species '" << species_type << "', dynamics_type='rrll' forcing radiating=True");
                thisSpecies->radiating=true;
            }
            
            if (!PyTools::extract("bc_part_type_west",thisSpecies->bc_part_type_west,"Species",ispec) )
                ERROR("For species '" << species_type << "', bc_part_type_west not defined");
            if (!PyTools::extract("bc_part_type_east",thisSpecies->bc_part_type_east,"Species",ispec) )
                ERROR("For species '" << species_type << "', bc_part_type_east not defined");
            
            if (params.nDim_particle>1) {
                if (!PyTools::extract("bc_part_type_south",thisSpecies->bc_part_type_south,"Species",ispec) )
                    ERROR("For species '" << species_type << "', bc_part_type_south not defined");
                if (!PyTools::extract("bc_part_type_north",thisSpecies->bc_part_type_north,"Species",ispec) )
                    ERROR("For species '" << species_type << "', bc_part_type_north not defined");
            }
            
            // for thermalizing BCs on particles check if thermT & thermVelocity is correctly defined
            // --------------------------------------------------------------------------------------
            bool thermTisDefined=false;
            bool thermVisDefined=false;
            
            // 1D: checks west/east bcs
            if ( (thisSpecies->bc_part_type_west=="thermalize") || (thisSpecies->bc_part_type_east=="thermalize") ){
                thermTisDefined=PyTools::extract("thermT",thisSpecies->thermT,"Species",ispec);
                if (!thermTisDefined) ERROR("thermT needs to be defined for species " <<ispec<< " due to x-BC thermalize");
                thermVisDefined=PyTools::extract("thermVelocity",thisSpecies->thermVelocity,"Species",ispec);
                if (!thermVisDefined) ERROR("thermVelocity needs to be defined for species " <<ispec<< " due to x-BC thermalize");
            }
            // 2D: checks south/north bcs
            if ( (params.nDim_particle==2) && (!thermTisDefined) && (!thermVisDefined) &&
                (thisSpecies->bc_part_type_south=="thermalize" || thisSpecies->bc_part_type_north=="thermalize") ) {
                thermTisDefined=PyTools::extract("thermT",thisSpecies->thermT,"Species",ispec);
                if (!thermTisDefined) ERROR("thermT needs to be defined for species " <<ispec<< " due to y-BC thermalize");
                thermVisDefined=PyTools::extract("thermVelocity",thisSpecies->thermVelocity,"Species",ispec);
                if (!thermTisDefined) ERROR("thermVelocity needs to be defined for species " <<ispec<< " due to y-BC thermalize");
                if (thisSpecies->thermVelocity.size()!=3) ERROR("thermVelocity needs to have all 3 components  defined for species " <<ispec)
            }
            
            if (thermTisDefined) {
                // if defined for only one direction thermT is put the same in all 3 directions (isotrop temperature)
                if (thisSpecies->thermT.size()==1) {
                    WARNING("Using thermT[0] for species " << species_type << " in all directions");
                    thisSpecies->thermT.resize(3);
                    for (unsigned int i=1; i<3;i++)
                        thisSpecies->thermT[i]=thisSpecies->thermT[0];
                }
                
            } else {
                // if not defined thermT & thermVelocity are put to 0
                thisSpecies->thermT.resize(3);
                for (unsigned int i=0; i<3;i++)
                    thisSpecies->thermT[i]=0.0;
                thisSpecies->thermVelocity.resize(3);
                for (unsigned int i=0; i<3;i++)
                    thisSpecies->thermVelocity[i]=0.0;
            }
            
            // Compute the thermalVelocity & Momentum for thermalizing bcs
            thisSpecies->thermalVelocity.resize(3);
            thisSpecies->thermalMomentum.resize(3);
            
            for (unsigned int i=0; i<3; i++) {
                thisSpecies->thermalVelocity[i] = sqrt(2.*thisSpecies->thermT[i]/thisSpecies->mass);
                thisSpecies->thermalMomentum[i] = thisSpecies->thermalVelocity[i];
                // Caution: momentum in SMILEI actually correspond to p/m
                if (thisSpecies->thermalVelocity[i]>0.3) ERROR("Thermalizing BCs for species " << species_type << " require non-relativistic thermT");
            }


            
            PyTools::extract("ionization_model", thisSpecies->ionization_model, "Species",ispec);
            
            if (thisSpecies->ionization_model != "none" && !PyTools::extract("atomic_number", thisSpecies->atomic_number, "Species",ispec)) {
                ERROR("For species '" << species_type << "', `atomic_number` not found => required for the ionization model .");
            }
            
            // Species geometry
            // ----------------
            
            // Density
            bool ok1, ok2;
            PyObject *profile1, *profile2, *profile3;
            ok1 = PyTools::extract_pyProfile("nb_density"    , profile1, "Species", ispec);
            ok2 = PyTools::extract_pyProfile("charge_density", profile1, "Species", ispec);
            if(  ok1 &&  ok2 ) ERROR("For species '" << species_type << "', cannot define both `nb_density` and `charge_density`.");
            if( !ok1 && !ok2 ) ERROR("For species '" << species_type << "', must define `nb_density` or `charge_density`.");
            if( ok1 ) thisSpecies->densityProfileType = "nb";
            if( ok2 ) thisSpecies->densityProfileType = "charge";
            
            thisSpecies->densityProfile = new Profile(profile1, params.nDim_particle, thisSpecies->densityProfileType+"_density "+species_type);
            
            // Number of particles per cell
            if( !PyTools::extract_pyProfile("n_part_per_cell", profile1, "Species", ispec))
                ERROR("For species '" << species_type << "', n_part_per_cell not found or not understood");
            thisSpecies->ppcProfile = new Profile(profile1, params.nDim_particle, "n_part_per_cell "+species_type);
            
            // Charge
            if( !PyTools::extract_pyProfile("charge", profile1, "Species", ispec))
                ERROR("For species '" << species_type << "', charge not found or not understood");
            thisSpecies->chargeProfile = new Profile(profile1, params.nDim_particle, "charge "+species_type);
            
            // Mean velocity
            PyTools::extract3Profiles("mean_velocity", ispec, profile1, profile2, profile3);
            thisSpecies->velocityProfile[0] = new Profile(profile1, params.nDim_particle, "mean_velocity[0] "+species_type);
            thisSpecies->velocityProfile[1] = new Profile(profile2, params.nDim_particle, "mean_velocity[1] "+species_type);
            thisSpecies->velocityProfile[2] = new Profile(profile3, params.nDim_particle, "mean_velocity[2] "+species_type);
            
            // Temperature
            PyTools::extract3Profiles("temperature", ispec, profile1, profile2, profile3);
            thisSpecies->temperatureProfile[0] = new Profile(profile1, params.nDim_particle, "temperature[0] "+species_type);
            thisSpecies->temperatureProfile[1] = new Profile(profile2, params.nDim_particle, "temperature[1] "+species_type);
            thisSpecies->temperatureProfile[2] = new Profile(profile3, params.nDim_particle, "temperature[2] "+species_type);
            
            
            // Extract test Species flag
            PyTools::extract("isTest", thisSpecies->particles.isTest, "Species", ispec);
            if (thisSpecies->particles.isTest) {
                // activate dump (might be changed below)
                thisSpecies->particles.track_every=1;
            }
            
            // check if particles have to be written and thus have to be labelled (Id property)
            PyTools::extract("track_every",thisSpecies->particles.track_every ,"Species",ispec);
            
            if (thisSpecies->particles.isTest && thisSpecies->particles.track_every == 0) {
                ERROR("For Species " << species_type << " isTest=True but track_every=0");
            }
            
            // Verify they don't ionize
            if (thisSpecies->ionization_model!="none" && thisSpecies->particles.isTest) {
                ERROR("For species '" << species_type << "', disabled for now : test & ionized");
            }
            
            // Create the particles
            if (!params.restart) {
                // unsigned int npart_effective=0;
                
                // Create particles in a space starting at cell_index
                std::vector<double> cell_index(3,0);
                for (unsigned int i=0 ; i<params.nDim_field ; i++) {
                    if (params.cell_length[i]!=0)
                        cell_index[i] = smpi->getDomainLocalMin(i);
                }
                
                int starting_bin_idx = 0;
                // does a loop over all cells in the simulation
                // considering a 3d volume with size n_space[0]*n_space[1]*n_space[2]
                /*npart_effective = */
                thisSpecies->createParticles(params.n_space, cell_index, starting_bin_idx );
                
                //PMESSAGE( 1, smpi->getRank(),"Species "<< speciesNumber <<" # part "<< npart_effective );
            }
            
            // Communicate some stuff if this is a species that has to be dumped (particles have Id)
            // Need to be placed after createParticles()
            if (thisSpecies->particles.track_every) {
                int locNbrParticles = thisSpecies->getNbrOfParticles();
                std::vector<int> allNbrParticles(smpi->smilei_sz);
                MPI_Gather( &locNbrParticles, 1, MPI_INTEGER, &allNbrParticles[0], 1, MPI_INTEGER, 0, MPI_COMM_WORLD );
                int nParticles(0);
                if (smpi->isMaster()) {
                    nParticles =  allNbrParticles[0];
                    for (int irk=1 ; irk<smpi->getSize() ; irk++){
                        allNbrParticles[irk] += nParticles;
                        nParticles = allNbrParticles[irk];
                    }
                    for (int irk=smpi->getSize()-1 ; irk>0 ; irk--){
                        allNbrParticles[irk] = allNbrParticles[irk-1];
                    }
                    allNbrParticles[0] = 0;
                }
                int offset(0);
                MPI_Scatter(&allNbrParticles[0], 1 , MPI_INTEGER, &offset, 1, MPI_INTEGER, 0, MPI_COMM_WORLD );
                thisSpecies->particles.addIdOffsets(offset);
            }
            
            // assign the correct Pusher to Push
            thisSpecies->Push = PusherFactory::create(params, thisSpecies);
            
            // Assign the Ionization model (if needed) to Ionize
            //  Needs to be placed after createParticles() because requires the knowledge of max_charge
            // \todo pay attention to restart
            thisSpecies->Ionize = IonizationFactory::create( params, thisSpecies);
            if (thisSpecies->Ionize) {
                DEBUG("Species " << species_type << " can be ionized!");
            }

            if (thisSpecies->Ionize && species_type=="electron") {
                ERROR("Species " << species_type << " can be ionized but species_type='electron'");
            }

            // define limits for BC and functions applied and for domain decomposition
            thisSpecies->partBoundCond = new PartBoundCond( params, thisSpecies, smpi);
            
            // Put the newly created species in the vector of species
            retSpecies.push_back(thisSpecies);
            
            // Print info
            unsigned int nPart = thisSpecies->getNbrOfParticles();
            MPI_Reduce(smpi->isMaster()?MPI_IN_PLACE:&nPart, &nPart, 1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);
            MESSAGE("Species " << ispec << " (" << species_type << ") created with " << nPart << " particles" );
        }
        
        // we cycle again to fix electron species for ionizable species
        for (unsigned int i=0; i<retSpecies.size(); i++) {
            if (retSpecies[i]->Ionize)  {
                Species *electron_species=NULL;
                for (unsigned int ispec=0; ispec<retSpecies.size(); ispec++) {
                    if (retSpecies[ispec]->species_type=="electron") {
                        if (electron_species) {
                            WARNING("Two species named electron : " << retSpecies[ispec]->speciesNumber << " and " << electron_species->speciesNumber);
                        } else {
                            electron_species=retSpecies[ispec];
                        }
                    }
                }
                if (!electron_species) {
                    for (unsigned int ispec=0; ispec<retSpecies.size(); ispec++) {
                        double charge=0;
                        PyTools::extract("charge",charge ,"Species",ispec);
                        if (retSpecies[ispec]->mass==1 && charge==-1) {
                            if (electron_species) {
                                WARNING("Two electron species: " << retSpecies[ispec]->species_type << " and " << electron_species->species_type);
                            } else {
                                electron_species=retSpecies[ispec];
                            }
                        }
                    }
                }
                if (electron_species) {
                    retSpecies[i]->electron_species=electron_species;
                    MESSAGE(1,"Ionization: Added " << electron_species->species_type << " species to species " << retSpecies[i]->species_type);
                } else {
                    ERROR("Ionization needs a species called \"electron\" to be defined");
                }
            }
        }
        return retSpecies;
    }

};

#endif
