#include "deconstructor.h"
#include "obdetails.h"
#include "framework.h"
#include "periodic.h"
#include "topology.h"

#include <string>
#include <sstream>
#include <queue>
#include <stack>
#include <set>
#include <utility>  // std::pair

#include <openbabel/babelconfig.h>
#include <openbabel/mol.h>
#include <openbabel/atom.h>
#include <openbabel/generic.h>
#include <openbabel/obconversion.h>
#include <openbabel/obiter.h>


namespace OpenBabel
{


std::string writeFragments(std::vector<OBMol> fragments, OBConversion obconv) {
	// Write a list of unique SMILES for a set of fragments
	// TODO: consider stripping out extraneous tabs, etc, here or elsewhere in the code.
	std::stringstream written;
	std::set<std::string> unique_smiles;
	for (std::vector<OBMol>::iterator it = fragments.begin(); it != fragments.end(); ++it) {
		unique_smiles.insert(getSMILES(*it, obconv));  // only adds unique values in a set
	}
	for (std::set<std::string>::iterator i2 = unique_smiles.begin(); i2 != unique_smiles.end(); ++i2) {
		written << *i2;
	}

	return written.str();
}


std::string getSMILES(OBMol fragment, OBConversion obconv) {
	// Prints SMILES based on OBConversion parameters
	OBMol canon = fragment;
	resetBonds(&canon);
	unwrapFragmentMol(&canon);
	return obconv.WriteString(&canon);
}



Deconstructor::Deconstructor(OBMol* orig_mof) : simplified_net(orig_mof) {
//Deconstructor::Deconstructor(OBMol* orig_mof) {
	parent_molp = orig_mof;
	// Avoid the copy constructor by using the member initializer list
	//simplified_net = Topology(parent_molp);  // TODO: does topology.h, etc., use Begin/EndModify routines?
	SetOutputDir(DEFAULT_OUTPUT_PATH);
	InitOutputFormat();
	infinite_node_detected = false;
}


void Deconstructor::InitOutputFormat() {
	// Allow overriding of obconv in derived classes
	obconv = OBConversion();
	obconv.SetOutFormat("can");  // Open Babel canonical SMILES
	obconv.AddOption("i");  // Ignore SMILES chirality for now

	/* Other potentially useful file formats, for derived classes: */
	// Universal SMILES:
	//obconv.SetOutFormat("smi");
	//obconv.AddOption("U");
	// InChI or InChIKey, with same flags as Universal SMILES:
	//obconv.SetOutFormat("inchi");
	//obconv.SetOutFormat("inchikey");
	//obconv.AddOption("F");
	//obconv.AddOption("M");
}



std::string Deconstructor::GetBasicSMILES(OBMol fragment) {
	// Get a standard, canonical SMILEs from an OBMol, e.g. to check for common fragments
	OBConversion basic_conv;
	basic_conv.SetOutFormat("can");  // Open Babel canonical SMILES
	basic_conv.AddOption("i");  // Ignore SMILES chirality for now
	return getSMILES(fragment, basic_conv);
}


void Deconstructor::DetectInitialNodesAndLinkers() {
	// Break apart the MOF to assign initial node/linker labels

	// Set up a temporary molecule for the initial metal-based fragmentation
	OBMol split_mol;
	copyMOF(parent_molp, &split_mol);

	// Find linkers by deleting bonds to metals
	// TODO: in this block, for SBU decomposition algorithms, do some manipulations to modify/restore bonds before fragmentation.
	// That will probably take the form of an optional preprocessing step before fragment assignment.
	// Similarly, there will probably be a fragmenter that breaks apart the nodes/linkers using a standard algorithm for node/linker SMILES names.
	std::vector<OBMol> fragments;
	deleteBonds(&split_mol, true);  // TODO: this could probably keep track of the deleted bond mapping
	fragments = split_mol.Separate();

	// Classify nodes and linkers based on composition.
	// Consider all single atoms and hydroxyl species as node building materials.
	std::stringstream nonmetalMsg;
	for (std::vector<OBMol>::iterator it = fragments.begin(); it != fragments.end(); ++it) {
		std::string mol_smiles = GetBasicSMILES(*it);
		VirtualMol fragment_act_atoms(parent_molp);
		fragment_act_atoms.ImportCopiedFragment(&*it);
		VirtualMol fragment_pa = simplified_net.OrigToPseudo(fragment_act_atoms);;

		// If str comparisons are required, include a "\t\n" in the proposed smiles
		bool all_oxygens = true;  // Also allow hydroxyls, etc.
		FOR_ATOMS_OF_MOL(a, *it){
			if (a->GetAtomicNum() != 8 && a->GetAtomicNum() != 1) {
				all_oxygens = false;
			}
		}

		if (it->NumAtoms() == 1) {
			nonmetalMsg << "Found a solitary atom with atomic number " << it->GetFirstAtom()->GetAtomicNum() << std::endl;
			simplified_net.SetRoleToAtoms( "node", fragment_pa);
		} else if (all_oxygens) {
			nonmetalMsg << "Found an oxygen species " << mol_smiles;
			simplified_net.SetRoleToAtoms("node", fragment_pa);  // consolidate neighboring metals in a later step
		} else {
			nonmetalMsg << "Deleting linker " << mol_smiles;
			simplified_net.SetRoleToAtoms("linker", fragment_pa);  // simplified later in CollapseLinkers()
		}
	}
	obErrorLog.ThrowError(__FUNCTION__, nonmetalMsg.str(), obDebug);
}


void Deconstructor::CollapseLinkers() {
	// Simplify the linker BB's into single points
	// (similar to CollapseNodes, but simpler without periodic chains)
	VirtualMol linker_pa = simplified_net.GetAtomsOfRole("linker");
	linker_pa = simplified_net.FragmentWithIntConns(linker_pa);
	std::vector<VirtualMol> linker_frags = linker_pa.Separate();
	for (std::vector<VirtualMol>::iterator it=linker_frags.begin(); it!=linker_frags.end(); ++it) {
		PseudoAtom collapsed = simplified_net.CollapseFragment(*it);
		simplified_net.SetRoleToAtom("linker", collapsed);
	}
}


bool Deconstructor::CollapseNodes() {
	// Simplify all the node SBUs into single points (handling rods)
	// Returns a flag indicating whether or not an infinite, MIL-like rod was detected

	bool mil_type_mof = false;
	VirtualMol node_pa = simplified_net.GetAtomsOfRole("node");
	node_pa = simplified_net.FragmentWithIntConns(node_pa);
	std::vector<VirtualMol> node_fragments = node_pa.Separate();
	for (std::vector<VirtualMol>::iterator it=node_fragments.begin(); it!=node_fragments.end(); ++it) {
		VirtualMol fragment_mol = *it;

		OBMol fragment_obmol = fragment_mol.ToOBMol();
		if (!isPeriodicChain(&fragment_obmol)) {  // normal, nonperiodic case
			PseudoAtom collapsed = simplified_net.CollapseFragment(*it);
			simplified_net.SetRoleToAtom("node", collapsed);
		} else {  // based on sepPeriodicChains
			// TODO: consider refactoring this code to a method within topology.cpp
			// or the upcoming simplification class
			obErrorLog.ThrowError(__FUNCTION__, "Detecting infinite chains", obInfo);
			mil_type_mof = true;

			// Detect single-atom nonmetal bridging atoms
			AtomSet bridging_atoms;
			AtomSet rod_atoms = it->GetAtoms();
			for (AtomSet::iterator rod_it=rod_atoms.begin(); rod_it!=rod_atoms.end(); ++rod_it) {
				AtomSet rod_orig = simplified_net.PseudoToOrig(VirtualMol(*rod_it)).GetAtoms();
				if (rod_orig.size() == 1) {
					OBAtom* single_atom = *(rod_orig.begin());
					if (!isMetal(single_atom)) {
						bridging_atoms.insert(*rod_it);  // the PA, not single_atom from the original MOF
					}
				}
			}

			// Handle bridging atoms separately from the rest of the rod.
			for (AtomSet::iterator br_it=bridging_atoms.begin(); br_it!=bridging_atoms.end(); ++br_it) {
				fragment_mol.RemoveAtom(*br_it);
				simplified_net.SetRoleToAtom("node bridge", *br_it);
			}
			fragment_mol = simplified_net.FragmentWithoutConns(fragment_mol);

			if (fragment_mol.NumAtoms() == 0) {
				obErrorLog.ThrowError(__FUNCTION__, "Unexpectedly deleted all atoms in a periodic rod during simplificaiton.", obError);
				continue;
			}

			// Simplify the non-bridging metals
			std::vector<VirtualMol> rod_fragments = fragment_mol.Separate();
			for (std::vector<VirtualMol>::iterator frag_it=rod_fragments.begin(); frag_it!=rod_fragments.end(); ++frag_it) {
				if (frag_it->NumAtoms() > 1) {
					obErrorLog.ThrowError(__FUNCTION__, "Combining metal atoms within a periodic rod (likely okay, but untested code--check it).", obError);
					fragment_mol = simplified_net.FragmentWithoutConns(fragment_mol);
					PseudoAtom collapsed = simplified_net.CollapseFragment(fragment_mol);
					simplified_net.SetRoleToAtom("node", collapsed);
				}  // else, if a single-metal fragment, there's nothing to simplify
			}

		}
	}

	return mil_type_mof;
}


void Deconstructor::SimplifyTopology() {
	// Simplify the topological net (series of steps including AxB, 1-c, etc.)

	int simplifications = 0;
	do {
		simplifications = 0;
		simplifications += simplified_net.SimplifyAxB();  // replacement for simplifyLX
		// collapseXX is no longer necessary now that we're properly tracking connections

		// Handle one-connected species, notably bound solvents and metal-containing ligands.
		// TODO: check the composition of free solvents and consider connecting charged anions back to the node
		AtomSet net_1c_without_conn = simplified_net.GetAtoms(false).GetAtoms();
		for (AtomSet::iterator it=net_1c_without_conn.begin(); it!=net_1c_without_conn.end(); ++it) {
			// Unlike the earlier algorithm, we can use the raw valence of the test point
			// because the SimplifyAxB method takes care of duplicate connections
			if ((*it)->GetValence() == 1) {
				// Find the neighbor of the 1-coordinated PA.
				// .begin() returns the first (in this case, only) element in the internal->external map.
				PseudoAtom it_conn = VirtualMol(*it).GetExternalBondsOrConns().begin()->second;
				VirtualMol it_and_conn = VirtualMol(*it);
				it_and_conn.AddAtom(it_conn);
				PseudoAtom nbor_of_1c = it_and_conn.GetExternalBondsOrConns().begin()->second;

				if (simplified_net.AtomHasRole(*it, "node")) {
					if (nbor_of_1c->GetValence() == 1) {
						obErrorLog.ThrowError(__FUNCTION__, "Not collapsing 1-c node into a 1-c linker", obInfo);
					} else {
						simplified_net.MergeAtomToAnother(*it, nbor_of_1c);
						++simplifications;
					}
				} else if (simplified_net.AtomHasRole(*it, "node bridge")) {
					// probably not uncommon due to PBC and unique OBAtoms
					continue;
				} else if (simplified_net.AtomHasRole(*it, "linker")) {
					// Bound ligands, such as capping agents or bound solvents for ASR removal.
					// TODO: consider if there are cases when the bound ligand should not be removed
					simplified_net.DeleteAtomAndConns(*it, "bound solvent");
					++simplifications;
				} else {
					obErrorLog.ThrowError(__FUNCTION__, "Unexpected atom role in the simplified net.", obWarning);
				}
			} else if ((*it)->GetValence() == 0) {
				// Free solvents are isolated without any external connections
				simplified_net.DeleteAtomAndConns(*it, "free solvent");
				++simplifications;
			}
		}
	} while(simplifications);  // repeat until self-consistent
}


int Deconstructor::CheckCatenation() {
	// Catenation: check that all interpenetrated nets contain identical components.
	// Returns the number of identified nets

	std::vector<VirtualMol> net_components = simplified_net.GetAtoms().Separate();
	std::string base_formula = "";
	if (net_components.size() == 0) {
		obErrorLog.ThrowError(__FUNCTION__, "No MOFs found in the simplified net.", obError);
	} else {
		VirtualMol orig_piece = simplified_net.PseudoToOrig(net_components[0]);
		base_formula = orig_piece.ToOBMol(false).GetFormula();
	}
	// Compare separate topology graphs based on the orig_mol molecular formula
	for (std::vector<VirtualMol>::iterator it=net_components.begin(); it!=net_components.end(); ++it) {
		VirtualMol orig_piece = simplified_net.PseudoToOrig(*it);
		std::string component_formula = orig_piece.ToOBMol(false).GetFormula();
		if (component_formula != base_formula) {
			std::string err_msg =
				"Inconsistency in catenated nets.  Simplified net fragment with formula\n" +
				component_formula + " does not match first entry " + base_formula;
			obErrorLog.ThrowError(__FUNCTION__, err_msg, obWarning);
		}
	}

	return net_components.size();
}


std::string Deconstructor::GetCatenationInfo(int num_nets) {
	// Get formatted string about catenation
	std::stringstream cat;
	cat << "Found " << num_nets << " simplified net(s)";
	return cat.str();
}


void Deconstructor::SimplifyMOF(bool write_intermediate_cifs) {
	// Runs a MOF simplfication, optionally writing intermediate CIFs

	if (write_intermediate_cifs) { WriteSimplifiedNet("simplified_test_orig.cif"); }
	DetectInitialNodesAndLinkers();
	CollapseLinkers();
	if (write_intermediate_cifs) { WriteSimplifiedNet("test_partial.cif"); }
	infinite_node_detected = CollapseNodes();
	if (write_intermediate_cifs) { WriteSimplifiedNet("test_with_simplified_nodes.cif"); }
	SimplifyTopology();
	PostSimplification();
}


void Deconstructor::SetOutputDir(const std::string &path) {
	output_dir = path;
}


void Deconstructor::WriteCIFs() {
	// Write out accessory files: the decomposed and simplified MOF, including bond orders.
	// Also write the Systre topology file.

	WriteAtomsOfRole("node", "nodes.cif");
	WriteAtomsOfRole("linker", "linkers.cif");
	WriteAtomsOfRole("node bridge", "node_bridges.cif");

	// Write out detected solvents
	simplified_net.GetDeletedOrigAtoms("free solvent").ToCIF(GetOutputPath("free_solvent.cif"));
	simplified_net.GetDeletedOrigAtoms("bound solvent").ToCIF(GetOutputPath("bound_solvent.cif"));

	// Also write out the original MOF, desolvated into -FSR and -ASR versions
	simplified_net.PseudoToOrig(simplified_net.GetAtoms(false)).ToCIF(GetOutputPath("mof_asr.cif"));
	VirtualMol mof_fsr = simplified_net.PseudoToOrig(simplified_net.GetAtoms(false));
	mof_fsr.AddVirtualMol(simplified_net.GetDeletedOrigAtoms("bound solvent"));
	mof_fsr.ToCIF(GetOutputPath("mof_fsr.cif"));

	// Export the simplified net
	simplified_net.ToSimplifiedCIF(GetOutputPath("simplified_topology_with_two_conn.cif"));
	simplified_net.WriteSystre(GetOutputPath("topology.cgd"));
}


std::string Deconstructor::GetMOFInfo() {
	// Print out the SMILES for nodes and linkers, and the detected catenation
	std::stringstream analysis;

	VirtualMol node_export = simplified_net.GetAtomsOfRole("node");
	// Handle node and node_bridge separately to match old test SMILES
	node_export = simplified_net.PseudoToOrig(node_export);
	OBMol node_mol = node_export.ToOBMol();
	analysis << writeFragments(node_mol.Separate(), obconv);

	VirtualMol node_bridge_export = simplified_net.GetAtomsOfRole("node bridge");
	node_bridge_export = simplified_net.PseudoToOrig(node_bridge_export);
	OBMol node_bridge_mol = node_bridge_export.ToOBMol();
	analysis << writeFragments(node_bridge_mol.Separate(), obconv);

	VirtualMol linker_export = simplified_net.GetAtomsOfRole("linker");
	linker_export = simplified_net.PseudoToOrig(linker_export);
	OBMol linker_mol = linker_export.ToOBMol();
	analysis << writeFragments(linker_mol.Separate(), obconv);

	analysis << GetCatenationInfo(CheckCatenation());
	return analysis.str();
}


std::string Deconstructor::GetOutputPath(const std::string &base_filename) {
	return output_dir + "/" + base_filename;
}


void Deconstructor::WriteSimplifiedNet(const std::string &base_filename) {
	simplified_net.ToSimplifiedCIF(GetOutputPath(base_filename));
}


void Deconstructor::WriteAtomsOfRole(const std::string &simplified_role, const std::string &base_filename) {
	// Writes a CIF containing the original atoms corresponding to a PA role of simplified_role.
	// If base_filename is not specified, use the simplified_role.cif

	std::string output_path = base_filename;
	if (simplified_role == "") {
		output_path = simplified_role + ".cif";
	}
	output_path = GetOutputPath(output_path);

	VirtualMol role_export = simplified_net.GetAtomsOfRole(simplified_role);
	role_export = simplified_net.PseudoToOrig(role_export);
	role_export.ToCIF(output_path);
}



MOFidDeconstructor::MOFidDeconstructor(OBMol* orig_mof) : Deconstructor(orig_mof) {
	// Note: MOFidDeconstructor would call the default constructor for Deconstructor,
	// not Deconstructor(orig_mof) unless specified above.
	// See also https://www.learncpp.com/cpp-tutorial/114-constructors-and-initialization-of-derived-classes/

	// Could initialize MOFidDeconstructor variables, etc., here
}


void MOFidDeconstructor::PostSimplification() {
	// Split 4-coordinated linkers into 3+3 by convention for MIL-47, etc.
	if (infinite_node_detected) {
		AtomSet for_net_4c = simplified_net.GetAtoms(false).GetAtoms();
		for (AtomSet::iterator it_4c=for_net_4c.begin(); it_4c!=for_net_4c.end(); ++it_4c) {
			PseudoAtom sq_4c = *it_4c;
			if (sq_4c->GetValence() == 4 && simplified_net.AtomHasRole(sq_4c, "linker")) {
				simplified_net.SplitFourVertexIntoTwoThree(sq_4c);
			}
		}
	}
}



SingleNodeDeconstructor::SingleNodeDeconstructor(OBMol* orig_mof) : Deconstructor(orig_mof) {
	points_of_extension = VirtualMol(orig_mof);
}


VirtualMol SingleNodeDeconstructor::GetNonmetalRingSubstituent(OBAtom* src) {
	// Traverses through neighbors (and their neighbors) using a BFS to get the
	// full set of atoms connected to src excluding rings and metals.
	// Returns a VirtualMol including src, even if neighbors are not found
	VirtualMol branch(src);
	int failed_branches = 0;  // number of branches with rings and/or metals
	std::queue<OBAtom*> to_visit;
	to_visit.push(src);

	while(!to_visit.empty()) {
		OBAtom* curr_atom = to_visit.front();
		to_visit.pop();

		FOR_NBORS_OF_ATOM(n, *curr_atom) {
			if (!branch.HasAtom(&*n) && !isMetal(&*n)) {
				if (n->IsInRing()) {
					++failed_branches;
				} else {
					to_visit.push(&*n);
				}
			}
		}
		branch.AddAtom(curr_atom);
	}

	if (failed_branches > 2) {  // allow the connections to the 2 primary ring atoms
		return VirtualMol(src);  // neighbor with specified composition does not exist
	}

	return branch;
}


std::pair<VirtualMol,VirtualMol> SingleNodeDeconstructor::CalculateNonmetalRing(OBAtom* a, OBAtom* b) {
	// Gets the nonmetal ring containing neighboring PA's a and b,
	// plus attached hydrogens and any non-ring substituents
	// (e.g. it won't grab a phenyl group or a fused ring).
	// Returns an empty molecule if a and b are not in the same ring.

	// The second VirtualMol has point(s) of extension.  The first VirtualMol is the ring minus those points.
	// (could use a custom struct later if we need more fields)

	// Verify that a and b are neighbors
	bool found_b_nbor = false;
	FOR_NBORS_OF_ATOM(n, *a) {
		if (&*n == b) {
			found_b_nbor = true;
		}
	}
	OBMol* parent_mol = b->GetParent();
	if (!found_b_nbor) {
		obErrorLog.ThrowError(__FUNCTION__, "OBAtom's a and b must be neighbors.", obError);
		// return empty molecules
		return std::pair<VirtualMol,VirtualMol>(VirtualMol(parent_mol), VirtualMol(parent_mol));
	}

	// First run a DFS to get find the ring from a to b and get the neighbors
	const int max_ring_size = 6;
	std::stack<OBAtom*> to_visit;
	std::map<OBAtom*, OBAtom*> seen;  // <current atom, previous node>
	std::map<OBAtom*, int> seen_count;  // <current atom, size of ring>
	to_visit.push(a);
	seen[a] = NULL;
	seen_count[a] = 1;

	while (!to_visit.empty()) {
		OBAtom* curr_atom = to_visit.top();
		to_visit.pop();

		if (curr_atom == b) {
			break;  // done finding the primary a/b ring
		} else if (seen_count[curr_atom] >= max_ring_size) {
			continue;  // don't continue down nonproductive paths
		}

		FOR_NBORS_OF_ATOM(n, *curr_atom) {
			if (!isMetal(&*n) && (seen.find(&*n) == seen.end())) {
				if (curr_atom == a && &*n == b) {
					continue;  // must traverse the ring
				}
				seen[&*n] = curr_atom;
				seen_count[&*n] = seen_count[curr_atom] + 1;
				to_visit.push(&*n);
			}
		}
	}

	if (seen.find(b) == seen.end()) {
		obErrorLog.ThrowError(__FUNCTION__, "Could not find a ring containing both atoms", obError);
		return std::pair<VirtualMol,VirtualMol>(VirtualMol(parent_mol), VirtualMol(parent_mol));
	}

	// Traverse back through the ring, filling in the side substituents
	VirtualMol ring(b);
	VirtualMol atoms_with_ring_nbors(b->GetParent());  // ring atoms with nbors that are part of rings, excluding a and b
	OBAtom* curr_ring_atom = seen[b];
	while (curr_ring_atom != a) {
		ring.AddAtom(curr_ring_atom);
		if (curr_ring_atom->GetValence() > 2) {  // has non-ring coordination
			VirtualMol substituents = GetNonmetalRingSubstituent(curr_ring_atom);
			if (substituents.NumAtoms() == 1) {  // single curr_ring_atom if connection has rings, metals, etc.
				atoms_with_ring_nbors.AddAtom(curr_ring_atom);
				ring.RemoveAtom(curr_ring_atom);
			} else {
				ring.AddVirtualMol(substituents);
			}
		}
		curr_ring_atom = seen[curr_ring_atom];
	}
	if (atoms_with_ring_nbors.NumAtoms() > 1) {  // allow one to connect to an organic nodular BB, etc.
		obErrorLog.ThrowError(__FUNCTION__, "Possibly found a fused ring.  Skipped attachments to other rings", obWarning);
	}

	return std::pair<VirtualMol,VirtualMol>(ring, atoms_with_ring_nbors);
}


void SingleNodeDeconstructor::DetectInitialNodesAndLinkers() {
	// Identify metal-containing SBU's in the MOF
	// Build up metal SBU's based on the metals + surrounding neighbors, then assign linkers to the rest.

	// Start node identification by detecting metals
	VirtualMol nodes(parent_molp);
	FOR_ATOMS_OF_MOL(a, *parent_molp) {
		if (isMetal(&*a)) {
			nodes.AddAtom(&*a);
		}
	}

	// Add shell of nearest neighbor atoms
	AtomSet node_metals = nodes.GetAtoms();
	for (AtomSet::iterator metal=node_metals.begin(); metal!=node_metals.end(); ++metal) {
		FOR_NBORS_OF_ATOM(a, **metal) {
			nodes.AddAtom(&*a);
		}
	}

	// Process oxygen and nitrogen NN more thoroughly to determine if they should be part of the
	// node SBU or the organic linker.  Also find points of extension, like the carboxylate carbon.
	AtomSet temp_node = nodes.GetAtoms();
	VirtualMol visited_bridges(parent_molp);  // avoids counting N bridges or carboxylates twice
	for (AtomSet::iterator it=temp_node.begin(); it!=temp_node.end(); ++it) {
		PseudoAtom nn = *it;
		if (visited_bridges.HasAtom(nn)) {
			continue;  // don't visit an atom bridge twice
		}

		if (nn->GetAtomicNum() == 8) {  // oxygen
			// Check coordination environment to classify as part of the metal oxide or "other"
			VirtualMol attached_hydrogens(nn->GetParent());
			OBAtom* attached_nonmetal = NULL;
			FOR_NBORS_OF_ATOM(n, *nn) {
				if (!nodes.HasAtom(&*n)) {  // metals and other bound oxygens
					if (n->GetAtomicNum() == 1) {
						attached_hydrogens.AddAtom(&*n);
					} else if (n->GetAtomicNum() == 8) {
						obErrorLog.ThrowError(__FUNCTION__, "Found a loosely bound peroxide, which will be handled by 1-c solvent codes.", obWarning);
					} else {
						if (attached_nonmetal) {  // O is already bound to a different nonmetal!
							obErrorLog.ThrowError(__FUNCTION__, "Found a metal-bound oxygen with multiple nonmetal neighbors!  Results may have inconsistencies.", obError);
						}
						attached_nonmetal = &*n;
					}
				}
			}

			if (!attached_nonmetal) {
				// Keep the oxygen and its bound H's
				nodes.AddVirtualMol(attached_hydrogens);
			} else {
				// Test the oxygen binding mode to classify it as part of node vs. linker
				OBAtom* oxygen_bridge = NULL;
				FOR_NBORS_OF_ATOM(n, *attached_nonmetal) {
					if ((temp_node.find(&*n) != temp_node.end()) && (nn != &*n)) {
						// Use temp_node because it's the original list of metals and NN's, not added carbons, rings, etc.
						// Avoids a bug when handling self-connected paddlewheels, e.g. hMOF-22242.
						// Note: the carboxylates will become "node bridges" in this instance to avoid a self-connected, periodic node.
						if (n->GetAtomicNum() != 8) {
							obErrorLog.ThrowError(__FUNCTION__, "Unexpectedly found an M-O-NM-NM-M bridge.  Excepted a second oxygen.", obError);
						} else {
							if (oxygen_bridge) {  // already defined
								obErrorLog.ThrowError(__FUNCTION__, "Unexpectedly found multiple O2's for M-O1-NM-O2-M.", obWarning);
							}
							oxygen_bridge = &*n;
						}
					}
				}
				if (!oxygen_bridge) {
					// Binding end-on, e.g. carboxylates with a single oxygen binding or catechols.
					// TODO: think about cases where an oxygen binds to multiple metal atoms.
					// Should the oxygen in that case be part of the node or linker?
					nodes.RemoveAtom(nn);  // not part of an SBU cluster
				} else {
					// If there is a bridge, include the carboxylate carbon as part of the SBU
					points_of_extension.AddAtom(attached_nonmetal);
					visited_bridges.AddAtom(nn);
					visited_bridges.AddAtom(oxygen_bridge);
					// Get hydrogens attached to the second oxygen atom
					FOR_NBORS_OF_ATOM(b_h, *oxygen_bridge) {
						if (b_h->GetAtomicNum() == 1) {
							attached_hydrogens.AddAtom(&*b_h);
						}
					}
					// Bound H's share the same fate/assignment as their oxygen atoms
					nodes.AddVirtualMol(attached_hydrogens);
				}
			}

		} else if (nn->GetAtomicNum() == 7) {  // nitrogen
			// Check for M-N-N-M (if the nitrogen a neighbor that's a NN to a metal)
			OBAtom* bridge_bw_metals = NULL;
			FOR_NBORS_OF_ATOM(n, *nn) {
				if (n->GetAtomicNum() == 7 && nodes.HasAtom(&*n)) {
					if (bridge_bw_metals) {  // already defined
						obErrorLog.ThrowError(__FUNCTION__, "Unexpectedly found N-N-N species in node.", obWarning);
					}
					bridge_bw_metals = &*n;
				}
			}
			if (!bridge_bw_metals) {  // Binding end-on, e.g. pillared MOFs, porphyrins, and probably amines
				nodes.RemoveAtom(nn);  // not part of an SBU cluster
			} else {  // If there is a bridge, grab the whole ring
				std::pair<VirtualMol,VirtualMol> ring_info = CalculateNonmetalRing(nn, bridge_bw_metals);
				nodes.AddVirtualMol(ring_info.first);
				points_of_extension.AddVirtualMol(ring_info.second);
				visited_bridges.AddAtom(nn);
				visited_bridges.AddAtom(bridge_bw_metals);
			}
		}
	}

	// Assign node PA's
	// TODO: consider adding a CollapseLinkers step to SimplifyMOF(), which separates the PA's and collapses
	VirtualMol node_pa = simplified_net.OrigToPseudo(nodes);
	simplified_net.SetRoleToAtoms("node", node_pa);

	// Assign linker type to PA's
	// The remainder of the atoms are either organic SBUs or 2-c linkers by definition.
	// Keep points of extension as part of the linkers to avoid issues with PoE-PoE bonds.
	// Incorporating them as linkers will automatically take care of details like 2-c sites.
	VirtualMol orig_linkers(parent_molp);
	FOR_ATOMS_OF_MOL(a, *parent_molp) {
		if (!nodes.HasAtom(&*a)) {
			orig_linkers.AddAtom(&*a);
		}
	}
	VirtualMol pa_linkers = simplified_net.OrigToPseudo(orig_linkers);
	simplified_net.SetRoleToAtoms("linker", pa_linkers);

	// TODO: consider rewriting the base MOFid separation algorithm to be additive like this one
}


void SingleNodeDeconstructor::WriteCIFs() {
	// Call base class exporter, plus the new outputs
	Deconstructor::WriteCIFs();
	points_of_extension.ToCIF(GetOutputPath("points_of_extension.cif"));
	WriteSBUs("node_sbus_no_ext_conns.cif", false);
	WriteSBUs("node_sbus_with_ext_conns.cif", true);
}


void SingleNodeDeconstructor::WriteSBUs(const std::string &base_filename, bool external_bond_pa) {
	// Write out the combined SBU's, including points of extension.
	// If external_bond_pa is true, also write out pseudoatoms corresponding to the next
	// external atom connected to the point of extension.

	// Combine SBU's from nodes and PoE's (assigned as linkers for topological convenience)
	VirtualMol nodes = simplified_net.PseudoToOrig(simplified_net.GetAtomsOfRole("node"));
	VirtualMol sbus = nodes;
	sbus.AddVirtualMol(points_of_extension);
	OBMol sbu_mol = sbus.ToOBMol();  // Copy POE's as new atoms

	// Map PoE's from the original MOF to sbu_mol
	std::map<OBAtom*, OBAtom*> v_to_mol_poe;
	AtomSet poe_set = points_of_extension.GetAtoms();
	for (AtomSet::iterator it=poe_set.begin(); it!=poe_set.end(); ++it) {
		v_to_mol_poe[*it] = atomInOtherMol(*it, &sbu_mol);
	}

	// Find PoE-PoE bonds then delete them
	FOR_BONDS_OF_MOL(b, *parent_molp) {
		OBAtom* begin = b->GetBeginAtom();
		OBAtom* end = b->GetEndAtom();
		if (points_of_extension.HasAtom(begin) && points_of_extension.HasAtom(end)) {
			OBAtom* mol_begin = v_to_mol_poe[begin];
			OBAtom* mol_end = v_to_mol_poe[end];
			sbu_mol.DeleteBond(sbu_mol.GetBond(mol_begin, mol_end));
		}
	}

	if (external_bond_pa) {
		OBUnitCell* uc = getPeriodicLattice(parent_molp);
		for (AtomSet::iterator it=poe_set.begin(); it!=poe_set.end(); ++it) {
			OBAtom* poe_orig_atom = *it;  // PoE in the original MOF OBMol
			FOR_NBORS_OF_ATOM(nbor, *poe_orig_atom) {
				if (!nodes.HasAtom(&*nbor)) {  // external
					// Calculate a distance 1/3 of the way from the PoE to the external atom to avoid overlapping atoms
					// when two POEs are next to each other in the MOF (e.g. POXHER_clean.cif)
					vector3 poe_pos = poe_orig_atom->GetVector();
					vector3 external_pos = uc->UnwrapCartesianNear(nbor->GetVector(), poe_pos);
					vector3 conn_pos = uc->WrapCartesianCoordinate((2.0*poe_pos + external_pos) / 3.0);

					PseudoAtom new_conn_atom = formAtom(&sbu_mol, conn_pos, POE_EXTERNAL_ELEMENT);
					OBAtom* poe_mol_atom = v_to_mol_poe[poe_orig_atom];
					formBond(&sbu_mol, poe_mol_atom, new_conn_atom, 1);
				}
			}
		}
	}

	writeCIF(&sbu_mol, GetOutputPath(base_filename));
}



void AllNodeDeconstructor::CollapseLinkers() {
	// Detect branch points in the linkers
	SingleNodeDeconstructor::CollapseLinkers();  // TODO: do not call the single node version once implemented.  It will need to be custom
	// TODO FIXME IMPLEMENT
}


} // end namespace OpenBabel
