#include "spatialsim/setInfoFunction.h"
#include "spatialsim/mystruct.h"
#include "spatialsim/initializeFunction.h"
#include "spatialsim/searchFunction.h"
#include "spatialsim/astFunction.h"
#include "sbml/SBMLTypes.h"
#include "sbml/packages/spatial/common/SpatialExtensionTypes.h"
#include "sbml/packages/spatial/extension/SpatialModelPlugin.h"
#include <vector>
#include <iostream>

using namespace std;
LIBSBML_CPP_NAMESPACE_USE

void setCompartmentInfo(Model *model, std::vector<variableInfo*> &varInfoList)
{
	ListOfCompartments *loc = model->getListOfCompartments();
	unsigned int numOfCompartments = static_cast<unsigned int>(model->getNumCompartments());
	unsigned int i;
	for (i = 0; i < numOfCompartments; i++) {
		Compartment *c = loc->get(i);
		variableInfo *info = new variableInfo;
		InitializeVarInfo(info);
		varInfoList.push_back(info);
		info->com = c;
		info->id = c->getId().c_str();
		if (model->getRule(info->id) == 0) {
			info->isResolved = true;
			info->isUniform = true;
			info->value = (c->isSetSize()) ? new double(c->getSize()) : new double(1.0);
		}
	}
}

void setSpeciesInfo(Model *model, std::vector<variableInfo*> &varInfoList, unsigned int volDimension, unsigned int memDimension, int Xindex, int Yindex, int Zindex)
{
	ListOfSpecies *los = model->getListOfSpecies();
	unsigned int numOfSpecies = static_cast<unsigned int>(model->getNumSpecies());
	unsigned int i;
	int X, Y, Z;
	unsigned int numOfVolIndexes = Xindex * Yindex * Zindex;
	for (i = 0; i < numOfSpecies; i++) {
		Species *s = los->get(i);
		SpatialSpeciesPlugin* splugin = static_cast<SpatialSpeciesPlugin*>(s->getPlugin("spatial"));
		//species have spatial extension
		if (splugin->getIsSpatial()) {
			variableInfo *info = new variableInfo;
			InitializeVarInfo(info);
			varInfoList.push_back(info);
			info->sp = s;
			info->com = model->getCompartment(s->getCompartment());
			info->id = s->getId().c_str();
			if (info->com->getSpatialDimensions()== volDimension) {
				info->inVol = true;
			} else if (info->com->getSpatialDimensions()== memDimension) {
				info->inVol = false;
			}
			//species value is specified by initial amount, initial value, rule or initial assignment
			//species is spatially defined

			if (s->isSetInitialAmount() || s->isSetInitialConcentration()) {//Initial Amount or Initial Concentration
				info->value = new double[numOfVolIndexes];
				fill_n(info->value, numOfVolIndexes, 0);
				//if (!s->isSetConstant() || !s->getConstant()) {
				info->delta = new double[4 * numOfVolIndexes];
				fill_n(info->delta, 4 * numOfVolIndexes, 0.0);
				//}
				if (s->isSetInitialAmount()) {//Initial Amount
					info->isResolved = true;
					for (Z = 0; Z < Zindex; Z++) {
						for (Y = 0; Y < Yindex; Y++) {
							for (X = 0; X < Xindex; X++) {
								info->value[Z * Yindex * Xindex + Y * Xindex + X] = s->getInitialAmount();
							}
						}
					}
				} else if (s->isSetInitialConcentration()) {//Initial Concentration
					for (Z = 0; Z < Zindex; Z++) {
						for (Y = 0; Y < Yindex; Y++) {
							for (X = 0; X < Xindex; X++) {
								info->value[Z * Yindex * Xindex + Y * Xindex + X] = s->getInitialConcentration();
							}
						}
					}
				}
			}
      info -> isResolved = true;
		}
	}
}

void setParameterInfo(Model *model, std::vector<variableInfo*> &varInfoList, int Xdiv, int Ydiv, int Zdiv, double &Xsize, double &Ysize, double &Zsize, double &deltaX, double &deltaY, double &deltaZ, char *&xaxis, char *&yaxis, char *&zaxis)
{
	SpatialModelPlugin *spPlugin = static_cast<SpatialModelPlugin*>(model->getPlugin("spatial"));
	Geometry *geometry = spPlugin->getGeometry();
	ListOfParameters *lop = model->getListOfParameters();
	unsigned int numOfParameters = static_cast<unsigned int>(model->getNumParameters());
	unsigned int i;
	int X, Y, Z;
	int Xindex = 2 * Xdiv - 1, Yindex = 2 * Ydiv - 1, Zindex = 2 * Zdiv - 1;//num of mesh
	int numOfVolIndexes = Xindex * Yindex * Zindex;
	string XmaxId = "", XminId = "", YmaxId = "", YminId = "", ZmaxId = "", ZminId = "";

	//id of coordinate boundary
	//cc = geometry->getCoordinateComponent(p->getId());
	for (i = 0; i < geometry->getNumCoordinateComponents(); i++) {
		CoordinateComponent *cc = geometry->getCoordinateComponent(i);
		if (cc->getType() ==  SPATIAL_COORDINATEKIND_CARTESIAN_X) {
			XmaxId = cc->getBoundaryMax()->getId();
			XminId = cc->getBoundaryMin()->getId();
		}
		if (cc->getType() ==  SPATIAL_COORDINATEKIND_CARTESIAN_Y) {
			YmaxId = cc->getBoundaryMax()->getId();
			YminId = cc->getBoundaryMin()->getId();
		}
		if (cc->getType() ==  SPATIAL_COORDINATEKIND_CARTESIAN_Z) {
			ZmaxId = cc->getBoundaryMax()->getId();
			ZminId = cc->getBoundaryMin()->getId();
		}
	}
	for (i = 0; i < numOfParameters; i++) {
		Parameter *p = lop->get(i);
		SpatialParameterPlugin *pPlugin = static_cast<SpatialParameterPlugin*>(p->getPlugin("spatial"));
		variableInfo *info = new variableInfo;
		BoundaryCondition *bcon;
		CoordinateComponent *cc;
		variableInfo *sInfo;
		InitializeVarInfo(info);
		varInfoList.push_back(info);
		info->para = p;
		info->id = p->getId().c_str();
		if (!pPlugin->isSpatialParameter()) {//normal parameters
			if (model->getRule(info->id) == 0 && p->isSetValue()) {
				info->isResolved = true;
				info->isUniform = true;
				info->value = new double(p->getValue());
			}
		} else {//spatial parameter plugin
			switch (pPlugin->getType()) {
			case SBML_SPATIAL_DIFFUSIONCOEFFICIENT://diffusion coefficient
			{
				sInfo = searchInfoById(varInfoList, pPlugin->getDiffusionCoefficient()->getVariable().c_str());
				if (sInfo->diffCInfo == 0) {
					sInfo->diffCInfo = new variableInfo*[3];
					fill_n(sInfo->diffCInfo, 3, reinterpret_cast<variableInfo*>(0));
				}

				DiffusionCoefficient* dc = pPlugin->getDiffusionCoefficient();
				switch (dc->getType()) {
				case SPATIAL_DIFFUSIONKIND_ISOTROPIC:
					sInfo->diffCInfo[0] = info;
					sInfo->diffCInfo[1] = info;
					if(Zindex > 1) sInfo->diffCInfo[2] = info;
					break;

				case SPATIAL_DIFFUSIONKIND_ANISOTROPIC:
					sInfo->diffCInfo[dc->getCoordinateReference1() - 1] = info;
					break;

				case SPATIAL_DIFFUSIONKIND_TENSOR:
					sInfo->diffCInfo[dc->getCoordinateReference1() - 1] = info;
					sInfo->diffCInfo[dc->getCoordinateReference2() - 1] = info;
					break;

				case DIFFUSIONKIND_UNKNOWN:
					cerr << "warning: Diffusion_Kind == Unknown" << endl;
					break;
				}

				if (model->getRule(info->id) == 0 && p->isSetValue()) {
					info->isResolved = true;
					info->isUniform = true;
					switch (dc->getType()) {
					case SPATIAL_DIFFUSIONKIND_ISOTROPIC:
						info->value = new double(p->getValue());
						break;
					case SPATIAL_DIFFUSIONKIND_ANISOTROPIC:
						info->value = new double(p->getValue());
						break;

					case SPATIAL_DIFFUSIONKIND_TENSOR:
						info->value = new double(p->getValue());
						break;

					case DIFFUSIONKIND_UNKNOWN:
						cerr << "warning: Diffusion_Kind == Unknown" << endl;
						break;
					}
				}
			}
			break;
			case SBML_SPATIAL_ADVECTIONCOEFFICIENT://advection coefficient
				sInfo = searchInfoById(varInfoList, pPlugin->getAdvectionCoefficient()->getVariable().c_str());
				if (sInfo->adCInfo == 0) {
					sInfo->adCInfo = new variableInfo*[3];
					fill_n(sInfo->adCInfo, 3, reinterpret_cast<variableInfo*>(0));
				}
				sInfo->adCInfo[pPlugin->getAdvectionCoefficient()->getCoordinate() - 1] = info;
				if (model->getRule(info->id) == 0 && p->isSetValue()) {
					info->isResolved = true;
					info->isUniform = true;
					info->value = new double(p->getValue());
				}
				break;
			case SBML_SPATIAL_BOUNDARYCONDITION://boundary condition
			{
				sInfo = searchInfoById(varInfoList, pPlugin->getBoundaryCondition()->getVariable().c_str());
				bcon = pPlugin->getBoundaryCondition();
				if (sInfo->boundaryInfo == 0) {
					sInfo->boundaryInfo = new variableInfo*[6];
					fill_n(sInfo->boundaryInfo, 6, reinterpret_cast<variableInfo*>(0));
				}
				if (sInfo != 0 &&bcon != 0) {
					int boundaryIndex = -1;
					if (bcon->getCoordinateBoundary() == XmaxId) boundaryIndex = Xmax;
					if (bcon->getCoordinateBoundary() == XminId) boundaryIndex = Xmin;
					if (bcon->getCoordinateBoundary() == YmaxId) boundaryIndex = Ymax;
					if (bcon->getCoordinateBoundary() == YminId) boundaryIndex = Ymin;
					if (bcon->getCoordinateBoundary() == ZmaxId) boundaryIndex = Zmax;
					if (bcon->getCoordinateBoundary() == ZminId) boundaryIndex = Zmin;
					if (boundaryIndex != -1) {
						sInfo->boundaryInfo[boundaryIndex] = info;
						if (model->getRule(info->id) == 0 && p->isSetValue()) {
							info->isResolved = true;
							info->isUniform = true;
							info->value = new double(p->getValue());
						}
					}
				}
			}
			break;
			case SBML_SPATIAL_SPATIALSYMBOLREFERENCE: {//spatial symbol reference
				                                   //  if (pPlugin->getSpatialSymbolReference()->getType() == "coordinateComponent")
				cc = geometry->getCoordinateComponent(pPlugin->getSpatialSymbolReference()->getSpatialRef());
				double min = cc->getBoundaryMin()->getValue();
				double max = cc->getBoundaryMax()->getValue();
				if (cc->getType() ==  SPATIAL_COORDINATEKIND_CARTESIAN_X) {
					info->value = new double[numOfVolIndexes];
					fill_n(info->value, numOfVolIndexes, 0);
					xaxis = const_cast<char*>(p->getId().c_str());
					Xsize = max - min;
					deltaX = (max - min) / (Xdiv - 1);
					info->isResolved = true;
					for (Z = 0; Z < Zindex; Z++) {
						for (Y = 0; Y < Yindex; Y++) {
							for (X = 0; X < Xindex; X++) {
								info->value[Z * Yindex * Xindex + Y * Xindex + X] = min + static_cast<double>(X) * deltaX / 2.0;
							}
						}
					}
				} else if (cc->getType() ==  SPATIAL_COORDINATEKIND_CARTESIAN_Y) {
					info->value = new double[numOfVolIndexes];
					fill_n(info->value, numOfVolIndexes, 0);
					yaxis = const_cast<char*>(p->getId().c_str());
					Ysize = max - min;
					deltaY = (max - min) / (Ydiv - 1);
					info->isResolved = true;
					for (Z = 0; Z < Zindex; Z++) {
						for (Y = 0; Y < Yindex; Y++) {
							for (X = 0; X < Xindex; X++) {
								info->value[Z * Yindex * Xindex + Y * Xindex + X] = min + static_cast<double>(Y) * deltaY / 2.0;
							}
						}
					}
				} else if (cc->getType() ==  SPATIAL_COORDINATEKIND_CARTESIAN_Z) {
					info->value = new double[numOfVolIndexes];
					fill_n(info->value, numOfVolIndexes, 0);
					zaxis = const_cast<char*>(p->getId().c_str());
					Zsize = max - min;
					deltaZ = (max - min) / (Zdiv - 1);
					info->isResolved = true;
					for (Z = 0; Z < Zindex; Z++) {
						for (Y = 0; Y < Yindex; Y++) {
							for (X = 0; X < Xindex; X++) {
								info->value[Z * Yindex * Xindex + Y * Xindex + X] = min + static_cast<double>(Z) * deltaZ / 2.0;
							}
						}
					}
				}
			}
			break;
			default:
				break;
			}
		}
	}
}

void setReactionInfo(Model *model, std::vector<variableInfo*> &varInfoList, vector<reactionInfo*> &rInfoList, vector<reactionInfo*> &fast_rInfoList, unsigned int numOfVolIndexes)
{
	ListOfReactions *lor = model->getListOfReactions();
	unsigned int numOfReactions = static_cast<unsigned int>(model->getNumReactions());
	unsigned int i, j, k;
	ASTNode *ast = 0;
	unsigned int numOfASTNodes = 0;
	Species *s;
	for (i = 0; i < numOfReactions; i++) {
		Reaction *r = lor->get(i);
		const KineticLaw *kl = r->getKineticLaw();
		if (kl != 0) {
			for (j = 0; j < kl->getNumLocalParameters(); j++) {
				const LocalParameter *lp = kl->getLocalParameter(j);
				variableInfo *info = new variableInfo;
				InitializeVarInfo(info);
				info->id = lp->getId().c_str();
				if (model->getRule(info->id) == 0 && lp->isSetValue()) {
					info->isResolved = true;
					info->isUniform = true;
					info->value = new double(lp->getValue());
				}
				varInfoList.push_back(info);
			}

      reactionInfo *rInfo = new reactionInfo;
      rInfo->reaction = r;
      for (j = 0; j < r->getNumReactants(); j++) {
        SpeciesReference *sr_re = r->getReactant(j);
        Species *reactant = model->getSpecies(sr_re->getSpecies());
        for (k = 0; k < r->getNumProducts(); k++) {
          SpeciesReference *sr_pro = r->getProduct(k);
          Species *product = model->getSpecies(sr_pro->getSpecies());
          if (reactant->getCompartment() != product->getCompartment()) {
            rInfo->isMemTransport = true;
            goto end;
          }
        }
      }
      rInfo->isMemTransport = false;
    end:
      for (j = 0; j < r->getNumReactants(); j++) {
        rInfo->spRefList.push_back(searchInfoById(varInfoList, r->getReactant(j)->getSpecies().c_str()));
        rInfo->srStoichiometry.push_back(r->getReactant(j)->getStoichiometry());
        s = model->getSpecies(r->getReactant(j)->getSpecies());
        if (!s->isSetConstant() || !s->getConstant() || !s->getBoundaryCondition()) {
          rInfo->isVariable.push_back(true);
        } else {
          rInfo->isVariable.push_back(false);
        }
      }
      for (j = 0; j < r->getNumProducts(); j++) {
        rInfo->spRefList.push_back(searchInfoById(varInfoList, r->getProduct(j)->getSpecies().c_str()));
        rInfo->srStoichiometry.push_back(r->getProduct(j)->getStoichiometry());
        s = model->getSpecies(r->getProduct(j)->getSpecies());
        if (!s->isSetConstant() || !s->getConstant() || !s->getBoundaryCondition()) {
          rInfo->isVariable.push_back(true);
        } else {
          rInfo->isVariable.push_back(false);
        }
      }
      //mashimo
      for (j = 0; j < r->getNumModifiers(); j++) {
        rInfo->spRefList.push_back(searchInfoById(varInfoList, r->getModifier(j)->getSpecies().c_str()));
        //rInfo->srStoichiometry.push_back();//Modifierはstoichiometry持ってない
        s = model->getSpecies(r->getModifier(j)->getSpecies());
        if (!s->isSetConstant() || !s->getConstant() || !s->getBoundaryCondition()) {
          rInfo->isVariable.push_back(true);
        } else {
          rInfo->isVariable.push_back(false);
        }
      }
      rInfo->id = r->getId().c_str();
      rInfo->value = new double[numOfVolIndexes];
      fill_n(rInfo->value, numOfVolIndexes, 0);
      ast = const_cast<ASTNode*>(kl->getMath());
      unsigned int tmp = 0;
      countAST(ast, tmp);
      //cout << "before reaction: " << SBML_formulaToString(ast) << endl;
      //cerr << "num_of_nodes: " << tmp << endl;
      rearrangeAST(ast);
      if (rInfo->isMemTransport == false) {
        cout << "reaction: " << SBML_formulaToString(ast) << endl;
      } else {
        cout << "mem trans: " << SBML_formulaToString(ast) << endl;
      }
      numOfASTNodes = 0;
      countAST(ast, numOfASTNodes);
      //cerr << "num_of_nodes: " << numOfASTNodes << endl;
      rInfo->rpInfo = new reversePolishInfo();
      rInfo->rpInfo->varList = new double*[numOfASTNodes];
      fill_n(rInfo->rpInfo->varList, numOfASTNodes, reinterpret_cast<double*>(0));
      rInfo->rpInfo->deltaList = new double*[numOfASTNodes];
      fill_n(rInfo->rpInfo->deltaList, numOfASTNodes, reinterpret_cast<double*>(0));
      rInfo->rpInfo->constList = new double*[numOfASTNodes];
      fill_n(rInfo->rpInfo->constList, numOfASTNodes, reinterpret_cast<double*>(0));
      rInfo->rpInfo->opfuncList = new int[numOfASTNodes];
      fill_n(rInfo->rpInfo->opfuncList, numOfASTNodes, 0);
      rInfo->rpInfo->listNum = numOfASTNodes;
      parseAST(ast, rInfo->rpInfo, varInfoList, numOfASTNodes);
      if (!r->getFast()) {
        rInfoList.push_back(rInfo);
      } else {
        fast_rInfoList.push_back(rInfo);
      }
    }
  }
}

void setRateRuleInfo(Model *model, std::vector<variableInfo*> &varInfoList, std::vector<reactionInfo*> &rInfoList, unsigned int numOfVolIndexes)
{
	unsigned int numOfRules = static_cast<unsigned int>(model->getNumRules());
	unsigned int i;
	ASTNode *ast = 0;
	unsigned int numOfASTNodes = 0;
	Species *s;
	for (i = 0; i < numOfRules; i++) {
		if (model->getRule(i)->isRate()) {
			RateRule *rrule = static_cast<RateRule*>(model->getRule(i));
			reactionInfo *rInfo = new reactionInfo;
			rInfo->id = rrule->getVariable().c_str();
			rInfo->value = new double[numOfVolIndexes];
			fill_n(rInfo->value, numOfVolIndexes, 0);
			ast = const_cast<ASTNode*>(rrule->getMath());
			rearrangeAST(ast);
			cout << "rate rule: " << SBML_formulaToString(ast) << endl;
			numOfASTNodes = 0;
			countAST(ast, numOfASTNodes);
			rInfo->rpInfo = new reversePolishInfo();
			rInfo->rpInfo->varList = new double*[numOfASTNodes];
			fill_n(rInfo->rpInfo->varList, numOfASTNodes, reinterpret_cast<double*>(0));
			rInfo->rpInfo->deltaList = new double*[numOfASTNodes];
			fill_n(rInfo->rpInfo->deltaList, numOfASTNodes, reinterpret_cast<double*>(0));
			rInfo->rpInfo->constList = new double*[numOfASTNodes];
			fill_n(rInfo->rpInfo->constList, numOfASTNodes, reinterpret_cast<double*>(0));
			rInfo->rpInfo->opfuncList = new int[numOfASTNodes];
			fill_n(rInfo->rpInfo->opfuncList, numOfASTNodes, 0);
			rInfo->rpInfo->listNum = numOfASTNodes;
			parseAST(ast, rInfo->rpInfo, varInfoList, numOfASTNodes);
			rInfo->spRefList.push_back(searchInfoById(varInfoList, rrule->getVariable().c_str()));
			rInfo->srStoichiometry.push_back(1.0);
			s = model->getSpecies(rrule->getVariable());
			if (!s->isSetConstant() || !s->getConstant() || !s->getBoundaryCondition()) {
				rInfo->isVariable.push_back(true);
			} else {
				rInfo->isVariable.push_back(false);
			}
			rInfoList.push_back(rInfo);
		}
	}
}

normalUnitVector* setNormalAngle(std::vector<GeometryInfo*> &geoInfoList, double Xsize, double Ysize, double Zsize, int dimension, int Xindex, int Yindex, int Zindex, unsigned int numOfVolIndexes)
{
  unsigned int i, j, k, step_kXY = 0, step_kYZ = 0, step_kXZ = 0;
  int X, Y, Z, index;
  normalUnitVector *nuVec = new normalUnitVector[numOfVolIndexes];
  int *isD = 0;
  GeometryInfo *geoInfo = 0;
  double X1 = 0.0, X2 = 0.0, Y1 = 0.0, Y2 = 0.0, Z1 = 0.0, Z2 = 0.0, len, rhoXY = 0.0, rhoYZ = 0.0, rhoXZ = 0.0;
  double a = 0.0, b = 0.0, c = 0.0;//length of triangle
  double max_radiusXY = 0.0, max_radiusYZ = 0.0, max_radiusXZ = 0.0;
  int startX, startY, startZ;
  int preD = -1;
  int Xdiv = (Xindex + 1) / 2;
  int Ydiv = (Yindex + 1) / 2;
  int Zdiv = (Zindex + 1) / 2;
  double hX = 1.0, hY = 1.0, hZ = 1.0;//grid size
  double hXY = 1.0, hYZ = 1.0, hXZ = 1.0;
  //double XYratio = Ysize / Xsize, XZratio = Zsize / Xsize;// unused varibale
  int xyPlaneX[2] = {0}, xyPlaneY[2] = {0}, yzPlaneY[2] = {0}, yzPlaneZ[2] = {0}, xzPlaneX[2] = {0}, xzPlaneZ[2] = {0};
	/*
	   hX = 1.0 / (Xdiv - 1);
	   hY = XYratio / (Ydiv - 1);
	   if (dimension == 2) {
	   hXY = (hX + hY) / 2.0;
	   } else if (dimension == 3) {
	   hZ = XZratio / (Zdiv - 1);
	   hXY = (hX + hY) / 2.0;
	   hYZ = (hY + hZ) / 2.0;
	   hXZ = (hX + hZ) / 2.0;
	   }
	 */

	hX = Xsize / (Xdiv - 1);
	hY = Ysize / (Ydiv - 1);
	if (dimension == 2) {
		hXY = (hX + hY) / 2.0;
	} else if (dimension == 3) {
		hZ = Zsize / (Zdiv - 1);
		hXY = (hX + hY) / 2.0;
		hYZ = (hY + hZ) / 2.0;
		hXZ = (hX + hZ) / 2.0;
	}

  if (dimension >= 2) {
    for (i = 0; i < geoInfoList.size(); i++) {
      geoInfo = geoInfoList[i];
      isD = geoInfo->isDomain;
      if (geoInfo->isVol == false) {//avol is membrane
        //calc max_radius
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          Z = index / (Xindex * Yindex);
          Y = (index - Z * Xindex * Yindex) / Xindex;
          X = index - Z * Xindex * Yindex - Y * Xindex;
          startX = X;
          startY = Y;
          startZ = Z;
          preD = -1;
          //calc max|r_i - r_j|
          max_radiusXY = 0.0;
          max_radiusYZ = 0.0;
          max_radiusXZ = 0.0;
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {//xy plane
            for (k = 0; k < geoInfo->domainIndex.size(); k++) {
              if ((Y + 2 < Yindex) &&
                  (isD[Z * Yindex * Xindex + (Y + 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y + 1) * Xindex + X] == 2 && preD != S)) {//north
                //X = X;//self assign
                Y = Y + 2;
                preD = N;
              } else if (isD[Z * Yindex * Xindex + (Y + 1) * Xindex + (X + 1)] == 1 && preD != SW) {//northeast
                X = X + 1;
                Y = Y + 1;
                preD = NE;
              } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X + 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X + 1)] == 2 && preD != W) {//east
                X = X + 2;
                //Y = Y;//self assign
                preD = E;
              } else if (isD[Z * Yindex * Xindex + (Y - 1) * Xindex + (X + 1)] == 1 && preD != NW) {//southeast
                X = X + 1;
                Y = Y - 1;
                preD = SE;
              } else if ((Y - 2 >= 0) &&
                         (isD[Z * Yindex * Xindex + (Y - 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y - 1) * Xindex + X] == 2 && preD != N)) {//south
                //X = X;//self assign
                Y = Y - 2;
                preD = S;
              } else if (isD[Z * Yindex * Xindex + (Y - 1) * Xindex + (X - 1)] == 1 && preD != NE) {//southwest
                X = X - 1;
                Y = Y - 1;
                preD = SW;
              } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X - 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X - 1)] == 2 && preD != E) {//west
                X = X - 2;
                //Y = Y;//self assign
                preD = W;
              } else if (isD[Z * Yindex * Xindex + (Y + 1) * Xindex + (X - 1)] == 1 && preD != SE) {//northwest
                X = X - 1;
                Y = Y + 1;
                preD = NW;
              }
              max_radiusXY = max(max_radiusXY, sqrt(pow(((X - startX) / 2) * hX, 2) + pow(((Y - startY) / 2) * hY, 2)) / 2.0);
              if (X == startX && Y == startY) {
                break;
              }
            }
          }
          X = startX;
          Y = startY;
          Z = startZ;
          preD = -1;
          if (dimension == 3) {
            if ((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {//yz plane
              for (k = 0; k < geoInfo->domainIndex.size(); k++) {
                if ((Z + 2 < Zindex) &&
                    (isD[(Z + 2) * Yindex * Xindex + Y * Xindex + X] == 1 && isD[(Z + 1) * Yindex * Xindex + Y * Xindex + X] == 2 && preD != S)) {//north
                  //Y = Y;//self assign
                  Z = Z + 2;
                  preD = N;
                } else if (isD[(Z + 1) * Yindex * Xindex + (Y + 1) * Xindex + X] == 1 && preD != SW) {//northeast
                  Y = Y + 1;
                  Z = Z + 1;
                  preD = NE;
                } else if (isD[Z * Yindex * Xindex + (Y + 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y + 1) * Xindex + X] == 2 && preD != W) {//east
                  Y = Y + 2;
                  //Z = Z;//self assign
                  preD = E;
                } else if (isD[(Z - 1) * Yindex * Xindex + (Y + 1) * Xindex + X] == 1 && preD != NW) {//southeast
                  Y = Y + 1;
                  Z = Z - 1;
                  preD = SE;
                } else if ((Z - 2 >= 0) &&
                           (isD[(Z - 2) * Yindex * Xindex + Y * Xindex + X] == 1 && isD[(Z - 1) * Yindex * Xindex + Y * Xindex + X] == 2 && preD != N)) {//south
                  //Y = Y;//self assign
                  Z = Z - 2;
                  preD = S;
                } else if (isD[(Z - 1) * Yindex * Xindex + (Y - 1) * Xindex + X] == 1 && preD != NE) {//southwest
                  Y = Y - 1;
                  Z = Z - 1;
                  preD = SW;
                } else if (isD[Z * Yindex * Xindex + (Y - 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y - 1) * Xindex + X] == 2 && preD != E) {//west
                  Y = Y - 2;
                  //Z = Z;//self assign
                  preD = W;
                } else if (isD[(Z + 1) * Yindex * Xindex + (Y - 1) * Xindex + X] == 1 && preD != SE) {//northwest
                  Y = Y - 1;
                  Z = Z + 1;
                  preD = NW;
                }
                max_radiusYZ = max(max_radiusYZ, sqrt(pow(((Y - startY) / 2) * hY, 2) + pow(((Z - startZ) / 2) * hZ, 2)) / 2.0);
                if (Y == startY && Z == startZ) {
                  break;
                }
              }
            }
            X = startX;
            Y = startY;
            Z = startZ;
            preD = -1;
            if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {//xz plane
              for (k = 0; k < geoInfo->domainIndex.size(); k++) {
                if ((Z + 2 < Zindex) &&
                    (isD[(Z + 2) * Yindex * Xindex + Y * Xindex + X] == 1 && isD[(Z + 1) * Yindex * Xindex + Y * Xindex + X] == 2 && preD != S)) {//north
                  //X = X;//self assign
                  Z = Z + 2;
                  preD = N;
                } else if (isD[(Z + 1) * Yindex * Xindex + Y * Xindex + (X + 1)] == 1 && preD != SW) {//northeast
                  X = X + 1;
                  Z = Z + 1;
                  preD = NE;
                } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X + 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X + 1)] == 2 && preD != W) {//east
                  X = X + 2;
                  //Z = Z;//self assign
                  preD = E;
                } else if (isD[(Z - 1) * Yindex * Xindex + Y * Xindex + (X + 1)] == 1 && preD != NW) {//southeast
                  X = X + 1;
                  Z = Z - 1;
                  preD = SE;
                } else if ((Z - 2 >= 0) &&
                           (isD[(Z - 2) * Yindex * Xindex + Y * Xindex + X] == 1 && isD[(Z - 1) * Yindex * Xindex + Y * Xindex + X] == 2 && preD != N)) {//south
                  //X = X;//self assign
                  Z = Z - 2;
                  preD = S;
                } else if (isD[(Z - 1) * Yindex * Xindex + Y * Xindex + (X - 1)] == 1 && preD != NE) {//southwest
                  X = X - 1;
                  Z = Z - 1;
                  preD = SW;
                } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X - 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X - 1)] == 2 && preD != E) {//west
                  X = X - 2;
                  //Z = Z;//self assign
                  preD = W;
                } else if (isD[(Z + 1) * Yindex * Xindex + Y * Xindex + (X - 1)] == 1 && preD != SE) {//northwest
                  X = X - 1;
                  Z = Z + 1;
                  preD = NW;
                }
                max_radiusXZ = max(max_radiusXZ, sqrt(pow(((X - startX) / 2) * hX, 2) + pow(((Z - startZ) / 2) * hZ, 2)) / 2.0);
                if (X == startX && Z == startZ) {
                  break;
                }
              }
            }
            X = startX;
            Y = startY;
            Z = startZ;
          }
          /* calc local radius of curvature */
          //calc tmp step_k to calc local radius of curvature
          step_kXY = static_cast<int>(pow(hXY, -(2.0 / 3.0)) + 0.5);
          step_kYZ = static_cast<int>(pow(hYZ, -(2.0 / 3.0)) + 0.5);
          step_kXZ = static_cast<int>(pow(hXZ, -(2.0 / 3.0)) + 0.5);
          //step_kXY = static_cast<int>(pow(hXY, -(2.0 / 3.0)));
          //step_kYZ = static_cast<int>(pow(hYZ, -(2.0 / 3.0)));
          //step_kXZ = static_cast<int>(pow(hXZ, -(2.0 / 3.0)));
          if (step_kXY == 0) step_kXY = 1;
          if (step_kYZ == 0) step_kYZ = 1;
          if (step_kXZ == 0) step_kXZ = 1;
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {//xy plane
            //calc the radius of circumscribed circle
            oneStepSearch(1, step_kXY, X, Y, Z, Xindex, Yindex, Zindex, xyPlaneX, xyPlaneY, isD, "xy");
            a = sqrt(pow(((X - xyPlaneX[0]) * hX) / 2.0, 2) + pow(((Y - xyPlaneY[0]) * hY) / 2.0, 2));
            b = sqrt(pow(((X - xyPlaneX[1]) * hX) / 2.0, 2) + pow(((Y - xyPlaneY[1]) * hY) / 2.0, 2));
            c = sqrt(pow(((xyPlaneX[0] - xyPlaneX[1]) * hX) / 2.0, 2) + pow(((xyPlaneY[0] - xyPlaneY[1]) * hY) / 2.0, 2));
            //calc step_k
            if ((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c) == 0) {//r_i+k, r_i, r_i-k are on the straight line
              step_kXY = 1;
              //step_kXY = static_cast<int>(pow((max_radiusXY / hXY), 2.0 / 3.0) + 0.5);
            } else {
              rhoXY = min(max_radiusXY, (a * b * c) / sqrt((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c)));
              step_kXY = static_cast<int>(pow((rhoXY / hXY), 2.0 / 3.0) + 0.5);
              //step_kXY = static_cast<int>(pow((rhoXY / hXY), 2.0 / 3.0));
            }
            //step_kXY = 1;
            //cout << X / 2 * hX << " " << Y / 2 * hY << " " << step_kXY << endl;
            //cout << X / 2 * hX << " " << Y / 2 * hY << " " << rhoXY << " " << max_radiusXY << " " << hXY << endl;
            //calc tangent vector
            oneStepSearch(1, step_kXY, X, Y, Z, Xindex, Yindex, Zindex, xyPlaneX, xyPlaneY, isD, "xy");
          }
          if (dimension == 3) {
            //yz-plane
            if ((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {//yz plane
              //calc the radius of circumscribed circle
              oneStepSearch(1, step_kYZ, X, Y, Z, Xindex, Yindex, Zindex, yzPlaneY, yzPlaneZ, isD, "yz");
              a = sqrt(pow(((Y - yzPlaneY[0]) * hY) / 2.0, 2) + pow(((Z - yzPlaneZ[0]) * hZ) / 2.0, 2));
              b = sqrt(pow(((Y - yzPlaneY[1]) * hY) / 2.0, 2) + pow(((Z - yzPlaneZ[1]) * hZ) / 2.0, 2));
              c = sqrt(pow(((yzPlaneY[0] - yzPlaneY[1]) * hY) / 2.0, 2) + pow(((yzPlaneZ[0] - yzPlaneZ[1]) * hZ) / 2.0, 2));
              //calc step_k
              if ((a + b + c) == 0 || (-a + b + c) == 0 || (a - b + c) == 0 || (a + b - c) == 0) {
                step_kYZ = 1;
                //step_kYZ = static_cast<int>(pow((max_radiusYZ / hYZ), 2.0 / 3.0) + 0.5);
              } else {
                rhoYZ = min(max_radiusYZ, (a * b * c) / sqrt((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c)));
                step_kYZ = static_cast<int>(pow((rhoYZ / hYZ), 2.0 / 3.0) + 0.5);
                //step_kYZ = static_cast<int>(pow((rhoYZ / hYZ), 2.0 / 3.0));
              }
              //calc tangent vector
              oneStepSearch(1, step_kYZ, X, Y, Z, Xindex, Yindex, Zindex, yzPlaneY, yzPlaneZ, isD, "yz");
            }
            //xz-plane
            if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {//xz plane
              //calc the radius of circumscribed circle
              oneStepSearch(1, step_kXZ, X, Y, Z, Xindex, Yindex, Zindex, xzPlaneX, xzPlaneZ, isD, "xz");
              a = sqrt(pow(((X - xzPlaneX[0]) * hX) / 2.0, 2) + pow(((Z - xzPlaneZ[0]) * hZ) / 2.0, 2));
              b = sqrt(pow(((X - xzPlaneX[1]) * hX) / 2.0, 2) + pow(((Z - xzPlaneZ[1]) * hZ) / 2.0, 2));
              c = sqrt(pow(((xzPlaneX[0] - xzPlaneX[1]) * hX) / 2.0, 2) + pow(((xzPlaneZ[0] - xzPlaneZ[1]) * hZ) / 2.0, 2));
              //calc step_k
              if ((a + b + c) == 0 || (-a + b + c) == 0 || (a - b + c) == 0 || (a + b - c) == 0) {
                step_kXZ = 1;
                //step_kXZ = static_cast<int>(pow((max_radiusXZ / hXZ), 2.0 / 3.0) + 0.5);
              } else {
                rhoXZ = min(max_radiusXZ, (a * b * c) / sqrt((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c)));
                step_kXZ = static_cast<int>(pow((rhoXZ / hXZ), 2.0 / 3.0) + 0.5);
                //step_kXZ = static_cast<int>(pow((rhoXZ / hXZ), 2.0 / 3.0));
              }
              //calc tangent vector
              oneStepSearch(1, step_kXZ, X, Y, Z, Xindex, Yindex, Zindex, xzPlaneX, xzPlaneZ, isD, "xz");
            }
          }

          //set normal unit vector
          //calc extended product
          if (dimension == 2) {
            X1 = xyPlaneX[0] - xyPlaneX[1];
            Y1 = xyPlaneY[0] - xyPlaneY[1];
            Z1 = 0.0;
            X2 = 0.0;
            Y2 = 0.0;
            Z2 = 1.0;
            //cout << "(" << xyPlaneX[0] << ", " << xyPlaneY[0] << ")   " << "(" << X << ", " << Y << ")   " << "(" << xyPlaneX[1] << ", " << xyPlaneY[1] << ")   " << endl;
          } else if (dimension == 3) {
            if (geoInfo->bType[index].isBofXp || geoInfo->bType[index].isBofXm) {//xy * xz
              X1 = xyPlaneX[0] - xyPlaneX[1];
              Y1 = xyPlaneY[0] - xyPlaneY[1];
              Z1 = 0.0;
              X2 = xzPlaneX[0] - xzPlaneX[1];
              Y2 = 0.0;
              Z2 = xzPlaneZ[0] - xzPlaneZ[1];
            }
            if (geoInfo->bType[index].isBofYp || geoInfo->bType[index].isBofYm) {//xy * yz
              X1 = xyPlaneX[0] - xyPlaneX[1];
              Y1 = xyPlaneY[0] - xyPlaneY[1];
              Z1 = 0.0;
              X2 = 0.0;
              Y2 = yzPlaneY[0] - yzPlaneY[1];
              Z2 = yzPlaneZ[0] - yzPlaneZ[1];
            }
            if (geoInfo->bType[index].isBofZp || geoInfo->bType[index].isBofZm) {//xz * yz
              X1 = xzPlaneX[0] - xzPlaneX[1];
              Y1 = 0.0;
              Z1 = xzPlaneZ[0] - xzPlaneZ[1];
              X2 = 0.0;
              Y2 = yzPlaneY[0] - yzPlaneY[1];
              Z2 = yzPlaneZ[0] - yzPlaneZ[1];
            }
            //これ、z境界かつy境界とかだと、あとが優先？
          }
          //cout << sqrt(pow(coord_x, 2) + pow(coord_y, 2) + pow(coord_z, 2)) << endl;
          nuVec[index].nx = Y1 * Z2 - Z1 * Y2;
          nuVec[index].ny = Z1 * X2 - X1 * Z2;
          nuVec[index].nz = X1 * Y2 - Y1 * X2;
          len = sqrt(pow(nuVec[index].nx, 2) + pow(nuVec[index].ny, 2) + pow(nuVec[index].nz, 2));
          //double phi = atan2(nuVec[index].ny, nuVec[index].nx);
          //double theta = acos(nuVec[index].nz / len);
          nuVec[index].nx /= len;
          nuVec[index].ny /= len;
          //nuVec[index].nx = sin(theta) * cos(phi);
          //nuVec[index].ny = sin(theta) * sin(phi);
          //cout << "    " << nuVec[index].nx << " " << nuVec[index].ny << " " << pow(nuVec[index].nx, 2) + pow(nuVec[index].ny, 2) <<endl;
          nuVec[index].nz /= len;
          //cout << "(" << X << ", " << Y << ", " << Z << ") " << nuVec[index].nx << " " << nuVec[index].ny << " " << nuVec[index].nz << endl;
          /*
            double coord_x = -1.0 + (double)X * hX / 2.0;
            double coord_y = -1.0 + (double)Y * hY / 2.0;
            double coord_z = -1.0 + (double)Z * hZ / 2.0;
            //cout << fabs(fabs(nuVec[index].nx) - fabs(coord_x)) + fabs(fabs(nuVec[index].ny) - fabs(coord_y)) + fabs(fabs(nuVec[index].nz) - fabs(coord_z)) << " ";
            nuVec[index].nx = coord_x;
            nuVec[index].ny = coord_y;
            nuVec[index].nz = coord_z;
          */
					//set normal unit vector
					//calc extended product
					if (dimension == 2) {
						X1 = xyPlaneX[0] - xyPlaneX[1];
						Y1 = xyPlaneY[0] - xyPlaneY[1];
						Z1 = 0.0;
						X2 = 0.0;
						Y2 = 0.0;
						Z2 = 1.0;
					} else if (dimension == 3) {
						if (geoInfo->bType[index].isBofXp || geoInfo->bType[index].isBofXm) {//xy * xz
							X1 = xyPlaneX[0] - xyPlaneX[1];
							Y1 = xyPlaneY[0] - xyPlaneY[1];
							Z1 = 0.0;
							X2 = xzPlaneX[0] - xzPlaneX[1];
							Y2 = 0.0;
							Z2 = xzPlaneZ[0] - xzPlaneZ[1];
						}
						if (geoInfo->bType[index].isBofYp || geoInfo->bType[index].isBofYm) {//xy * yz
							X1 = xyPlaneX[0] - xyPlaneX[1];
							Y1 = xyPlaneY[0] - xyPlaneY[1];
							Z1 = 0.0;
							X2 = 0.0;
							Y2 = yzPlaneY[0] - yzPlaneY[1];
							Z2 = yzPlaneZ[0] - yzPlaneZ[1];
						}
						if (geoInfo->bType[index].isBofZp || geoInfo->bType[index].isBofZm) {//xz * yz
							X1 = xzPlaneX[0] - xzPlaneX[1];
							Y1 = 0.0;
							Z1 = xzPlaneZ[0] - xzPlaneZ[1];
							X2 = 0.0;
							Y2 = yzPlaneY[0] - yzPlaneY[1];
							Z2 = yzPlaneZ[0] - yzPlaneZ[1];
						}
					}
					nuVec[index].nx = Y1 * Z2 - Z1 * Y2;
					nuVec[index].ny = Z1 * X2 - X1 * Z2;
					nuVec[index].nz = X1 * Y2 - Y1 * X2;
					len = sqrt(pow(nuVec[index].nx, 2) + pow(nuVec[index].ny, 2) + pow(nuVec[index].nz, 2));
					nuVec[index].nx /= len;
					nuVec[index].ny /= len;
					nuVec[index].nz /= len;
				}
			}
		}
	}
	return nuVec;
}

normalUnitVector* setNormalAngle_modify(std::vector<GeometryInfo*> &geoInfoList, double Xsize, double Ysize, double Zsize, int dimension, int Xindex, int Yindex, int Zindex, unsigned int numOfVolIndexes)
{
  unsigned int i, j, k, step_kXY = 0, step_kYZ = 0, step_kXZ = 0;
  int X, Y, Z, index;
  normalUnitVector *nuVec = new normalUnitVector[numOfVolIndexes];
  int *isD = 0;
  GeometryInfo *geoInfo = 0;
  double X1 = 0.0, X2 = 0.0, Y1 = 0.0, Y2 = 0.0, Z1 = 0.0, Z2 = 0.0, len, rhoXY = 0.0, rhoYZ = 0.0, rhoXZ = 0.0;
  double a = 0.0, b = 0.0, c = 0.0;//length of triangle
  double max_radiusXY = 0.0, max_radiusYZ = 0.0, max_radiusXZ = 0.0;
  int startX, startY, startZ;
  int preD = -1;
  int Xdiv = (Xindex + 1) / 2;
  int Ydiv = (Yindex + 1) / 2;
  int Zdiv = (Zindex + 1) / 2;
  double hX = 1.0, hY = 1.0, hZ = 1.0;//grid size
  double hXY = 1.0, hYZ = 1.0, hXZ = 1.0;
  int xyPlaneX[2] = {0}, xyPlaneY[2] = {0}, yzPlaneY[2] = {0}, yzPlaneZ[2] = {0}, xzPlaneX[2] = {0}, xzPlaneZ[2] = {0};
  hX = Xsize / (Xdiv - 1);
  hY = Ysize / (Ydiv - 1);
  if (dimension == 2) {
    hXY = (hX + hY) / 2.0;
  } else if (dimension == 3) {
    hZ = Zsize / (Zdiv - 1);
    hXY = (hX + hY) / 2.0;
    hYZ = (hY + hZ) / 2.0;
    hXZ = (hX + hZ) / 2.0;
  }

  if (dimension == 2) {
    for (i = 0; i < geoInfoList.size(); i++) {
      geoInfo = geoInfoList[i];
      isD = geoInfo->isDomain;
      if (geoInfo->isVol == false) {//avol is membrane
        //calc max_radius
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          Z = index / (Xindex * Yindex);
          Y = (index - Z * Xindex * Yindex) / Xindex;
          X = index - Z * Xindex * Yindex - Y * Xindex;
          startX = X;
          startY = Y;
          startZ = Z;
          preD = -1;
          //calc max|r_i - r_j|
          max_radiusXY = 0.0;
          max_radiusYZ = 0.0;
          max_radiusXZ = 0.0;
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {//xy plane
            for (k = 0; k < geoInfo->domainIndex.size(); k++) {
              if ((Y + 2 < Yindex) &&
                  (isD[Z * Yindex * Xindex + (Y + 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y + 1) * Xindex + X] == 2 && preD != S)) {//north
                //X = X;//self assign
                Y = Y + 2;
                preD = N;
              } else if (isD[Z * Yindex * Xindex + (Y + 1) * Xindex + (X + 1)] == 1 && preD != SW) {//northeast
                X = X + 1;
                Y = Y + 1;
                preD = NE;
              } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X + 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X + 1)] == 2 && preD != W) {//east
                X = X + 2;
                //Y = Y;//self assign
                preD = E;
              } else if (isD[Z * Yindex * Xindex + (Y - 1) * Xindex + (X + 1)] == 1 && preD != NW) {//southeast
                X = X + 1;
                Y = Y - 1;
                preD = SE;
              } else if ((Y - 2 >= 0) &&
                         (isD[Z * Yindex * Xindex + (Y - 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y - 1) * Xindex + X] == 2 && preD != N)) {//south
                //X = X;//self assign
                Y = Y - 2;
                preD = S;
              } else if (isD[Z * Yindex * Xindex + (Y - 1) * Xindex + (X - 1)] == 1 && preD != NE) {//southwest
                X = X - 1;
                Y = Y - 1;
                preD = SW;
              } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X - 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X - 1)] == 2 && preD != E) {//west
                X = X - 2;
                //Y = Y;//self assign
                preD = W;
              } else if (isD[Z * Yindex * Xindex + (Y + 1) * Xindex + (X - 1)] == 1 && preD != SE) {//northwest
                X = X - 1;
                Y = Y + 1;
                preD = NW;
              }
              max_radiusXY = max(max_radiusXY, sqrt(pow(((X - startX) / 2) * hX, 2) + pow(((Y - startY) / 2) * hY, 2)) / 2.0);
              if (X == startX && Y == startY) {
                break;
              }
            }
          }
          X = startX;
          Y = startY;
          Z = startZ;
          preD = -1;

          /* calc local radius of curvature */
          //calc tmp step_k to calc local radius of curvature
          step_kXY = static_cast<int>(pow(hXY, -(2.0 / 3.0)) + 0.5);
          step_kYZ = static_cast<int>(pow(hYZ, -(2.0 / 3.0)) + 0.5);
          step_kXZ = static_cast<int>(pow(hXZ, -(2.0 / 3.0)) + 0.5);
          if (step_kXY == 0) step_kXY = 1;
          if (step_kYZ == 0) step_kYZ = 1;
          if (step_kXZ == 0) step_kXZ = 1;
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {//xy plane
            //calc the radius of circumscribed circle
            oneStepSearch(1, step_kXY, X, Y, Z, Xindex, Yindex, Zindex, xyPlaneX, xyPlaneY, isD, "xy");
            a = sqrt(pow(((X - xyPlaneX[0]) * hX) / 2.0, 2) + pow(((Y - xyPlaneY[0]) * hY) / 2.0, 2));
            b = sqrt(pow(((X - xyPlaneX[1]) * hX) / 2.0, 2) + pow(((Y - xyPlaneY[1]) * hY) / 2.0, 2));
            c = sqrt(pow(((xyPlaneX[0] - xyPlaneX[1]) * hX) / 2.0, 2) + pow(((xyPlaneY[0] - xyPlaneY[1]) * hY) / 2.0, 2));
            //calc step_k
            if ((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c) == 0) {//r_i+k, r_i, r_i-k are on the straight line
              step_kXY = 1;
              //step_kXY = static_cast<int>(pow((max_radiusXY / hXY), 2.0 / 3.0) + 0.5);
            } else {
              rhoXY = min(max_radiusXY, (a * b * c) / sqrt((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c)));
              step_kXY = static_cast<int>(pow((rhoXY / hXY), 2.0 / 3.0) + 0.5);
              //step_kXY = static_cast<int>(pow((rhoXY / hXY), 2.0 / 3.0));
            }
            //step_kXY = 1;
            //cout << X / 2 * hX << " " << Y / 2 * hY << " " << step_kXY << endl;
            //cout << X / 2 * hX << " " << Y / 2 * hY << " " << rhoXY << " " << max_radiusXY << " " << hXY << endl;
            //calc tangent vector
            oneStepSearch(1, step_kXY, X, Y, Z, Xindex, Yindex, Zindex, xyPlaneX, xyPlaneY, isD, "xy");
          }

          //set normal unit vector
          //calc extended product
          if (geoInfo->bType[index].isBofXp || geoInfo->bType[index].isBofXm || (geoInfo->bType[index].isBofYp || geoInfo->bType[index].isBofYm)) {//xy * yz
            X1 = xyPlaneX[0] - xyPlaneX[1];
            Y1 = xyPlaneY[0] - xyPlaneY[1];
            Z1 = 0.0;
            X2 = 0.0;
            Y2 = 0.0;
            Z2 = 1.0;
          }
          nuVec[index].nx = Y1 * Z2 - Z1 * Y2;
          nuVec[index].ny = Z1 * X2 - X1 * Z2;
          nuVec[index].nz = X1 * Y2 - Y1 * X2;;
          len = sqrt(pow(nuVec[index].nx, 2) + pow(nuVec[index].ny, 2) + pow(nuVec[index].nz, 2));
          nuVec[index].nx /= len;
          nuVec[index].ny /= len;
          nuVec[index].nz /= len;
        }
      }
    }
  }

  if (dimension == 3) {
    for (i = 0; i < geoInfoList.size(); i++) {
      geoInfo = geoInfoList[i];
      isD = geoInfo->isDomain;
      if (geoInfo->isVol == false) {//avol is membrane
        //calc max_radius
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          Z = index / (Xindex * Yindex);
          Y = (index - Z * Xindex * Yindex) / Xindex;
          X = index - Z * Xindex * Yindex - Y * Xindex;
          startX = X;
          startY = Y;
          startZ = Z;
          preD = -1;
          max_radiusXY = 0.0;
          max_radiusYZ = 0.0;
          max_radiusXZ = 0.0;
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) && !(geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {//xy plane
            for (k = 0; k < geoInfo->domainIndex.size(); k++) {
              if ((Y + 2 < Yindex) &&
                  (isD[Z * Yindex * Xindex + (Y + 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y + 1) * Xindex + X] == 2 && preD != S)) {//north
                Y = Y + 2;
                preD = N;
              } else if (isD[Z * Yindex * Xindex + (Y + 1) * Xindex + (X + 1)] == 1 && preD != SW) {//northeast
                X = X + 1;
                Y = Y + 1;
                preD = NE;
              } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X + 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X + 1)] == 2 && preD != W) {//east
                X = X + 2;
                preD = E;
              } else if (isD[Z * Yindex * Xindex + (Y - 1) * Xindex + (X + 1)] == 1 && preD != NW) {//southeast
                X = X + 1;
                Y = Y - 1;
                preD = SE;
              } else if ((Y - 2 >= 0) &&
                  (isD[Z * Yindex * Xindex + (Y - 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y - 1) * Xindex + X] == 2 && preD != N)) {//south
                Y = Y - 2;
                preD = S;
              } else if (isD[Z * Yindex * Xindex + (Y - 1) * Xindex + (X - 1)] == 1 && preD != NE) {//southwest
                X = X - 1;
                Y = Y - 1;
                preD = SW;
              } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X - 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X - 1)] == 2 && preD != E) {//west
                X = X - 2;
                preD = W;
              } else if (isD[Z * Yindex * Xindex + (Y + 1) * Xindex + (X - 1)] == 1 && preD != SE) {//northwest
                X = X - 1;
                Y = Y + 1;
                preD = NW;
              }
              max_radiusXY = max(max_radiusXY, sqrt(pow(((X - startX) / 2) * hX, 2) + pow(((Y - startY) / 2) * hY, 2)) / 2.0);
              if (X == startX && Y == startY) {
                break;
              }
            }
          }
          X = startX;
          Y = startY;
          Z = startZ;
          preD = -1;
          if (((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) && !(geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm)) {//yz plane
            for (k = 0; k < geoInfo->domainIndex.size(); k++) {
              if ((Z + 2 < Zindex) &&
                  (isD[(Z + 2) * Yindex * Xindex + Y * Xindex + X] == 1 && isD[(Z + 1) * Yindex * Xindex + Y * Xindex + X] == 2 && preD != S)) {//north
                Z = Z + 2;
                preD = N;
              } else if (isD[(Z + 1) * Yindex * Xindex + (Y + 1) * Xindex + X] == 1 && preD != SW) {//northeast
                Y = Y + 1;
                Z = Z + 1;
                preD = NE;
              } else if (isD[Z * Yindex * Xindex + (Y + 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y + 1) * Xindex + X] == 2 && preD != W) {//east
                Y = Y + 2;
                preD = E;
              } else if (isD[(Z - 1) * Yindex * Xindex + (Y + 1) * Xindex + X] == 1 && preD != NW) {//southeast
                Y = Y + 1;
                Z = Z - 1;
                preD = SE;
              } else if ((Z - 2 >= 0) &&
                  (isD[(Z - 2) * Yindex * Xindex + Y * Xindex + X] == 1 && isD[(Z - 1) * Yindex * Xindex + Y * Xindex + X] == 2 && preD != N)) {//south
                Z = Z - 2;
                preD = S;
              } else if (isD[(Z - 1) * Yindex * Xindex + (Y - 1) * Xindex + X] == 1 && preD != NE) {//southwest
                Y = Y - 1;
                Z = Z - 1;
                preD = SW;
              } else if (isD[Z * Yindex * Xindex + (Y - 2) * Xindex + X] == 1 && isD[Z * Yindex * Xindex + (Y - 1) * Xindex + X] == 2 && preD != E) {//west
                Y = Y - 2;
                preD = W;
              } else if (isD[(Z + 1) * Yindex * Xindex + (Y - 1) * Xindex + X] == 1 && preD != SE) {//northwest
                Y = Y - 1;
                Z = Z + 1;
                preD = NW;
              }
              max_radiusYZ = max(max_radiusYZ, sqrt(pow(((Y - startY) / 2) * hY, 2) + pow(((Z - startZ) / 2) * hZ, 2)) / 2.0);
              if (Y == startY && Z == startZ) {
                break;
              }
            }
          }
          X = startX;
          Y = startY;
          Z = startZ;
          preD = -1;
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) && !(geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {//xz plane
            for (k = 0; k < geoInfo->domainIndex.size(); k++) {
              if ((Z + 2 < Zindex) &&
                  (isD[(Z + 2) * Yindex * Xindex + Y * Xindex + X] == 1 && isD[(Z + 1) * Yindex * Xindex + Y * Xindex + X] == 2 && preD != S)) {//north
                Z = Z + 2;
                preD = N;
              } else if (isD[(Z + 1) * Yindex * Xindex + Y * Xindex + (X + 1)] == 1 && preD != SW) {//northeast
                X = X + 1;
                Z = Z + 1;
                preD = NE;
              } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X + 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X + 1)] == 2 && preD != W) {//east
                X = X + 2;
                preD = E;
              } else if (isD[(Z - 1) * Yindex * Xindex + Y * Xindex + (X + 1)] == 1 && preD != NW) {//southeast
                X = X + 1;
                Z = Z - 1;
                preD = SE;
              } else if ((Z - 2 >= 0) &&
                  (isD[(Z - 2) * Yindex * Xindex + Y * Xindex + X] == 1 && isD[(Z - 1) * Yindex * Xindex + Y * Xindex + X] == 2 && preD != N)) {//south
                Z = Z - 2;
                preD = S;
              } else if (isD[(Z - 1) * Yindex * Xindex + Y * Xindex + (X - 1)] == 1 && preD != NE) {//southwest
                X = X - 1;
                Z = Z - 1;
                preD = SW;
              } else if (isD[Z * Yindex * Xindex + Y * Xindex + (X - 2)] == 1 && isD[Z * Yindex * Xindex + Y * Xindex + (X - 1)] == 2 && preD != E) {//west
                X = X - 2;
                preD = W;
              } else if (isD[(Z + 1) * Yindex * Xindex + Y * Xindex + (X - 1)] == 1 && preD != SE) {//northwest
                X = X - 1;
                Z = Z + 1;
                preD = NW;
              }
              max_radiusXZ = max(max_radiusXZ, sqrt(pow(((X - startX) / 2) * hX, 2) + pow(((Z - startZ) / 2) * hZ, 2)) / 2.0);
              if (X == startX && Z == startZ) {
                break;
              }
            }
          }
          X = startX;
          Y = startY;
          Z = startZ;
        }

        /* calc local radius of curvature */
        //calc tmp step_k to calc local radius of curvature
        step_kXY = static_cast<int>(pow(hXY, -(2.0 / 3.0)) + 0.5);
        step_kYZ = static_cast<int>(pow(hYZ, -(2.0 / 3.0)) + 0.5);
        step_kXZ = static_cast<int>(pow(hXZ, -(2.0 / 3.0)) + 0.5);
        if (step_kXY == 0) step_kXY = 1;
        if (step_kYZ == 0) step_kYZ = 1;
        if (step_kXZ == 0) step_kXZ = 1;
        /* xy plane */
        if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) && !(geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {
          //calc the radius of circumscribed circle
          oneStepSearch(1, step_kXY, X, Y, Z, Xindex, Yindex, Zindex, xyPlaneX, xyPlaneY, isD, "xy");
          a = sqrt(pow(((X - xyPlaneX[0]) * hX) / 2.0, 2) + pow(((Y - xyPlaneY[0]) * hY) / 2.0, 2));
          b = sqrt(pow(((X - xyPlaneX[1]) * hX) / 2.0, 2) + pow(((Y - xyPlaneY[1]) * hY) / 2.0, 2));
          c = sqrt(pow(((xyPlaneX[0] - xyPlaneX[1]) * hX) / 2.0, 2) + pow(((xyPlaneY[0] - xyPlaneY[1]) * hY) / 2.0, 2));
          //calc step_k
          if ((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c) == 0) {//r_i+k, r_i, r_i-k are on the straight line
            step_kXY = 1;
          } else {
            rhoXY = min(max_radiusXY, (a * b * c) / sqrt((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c)));
            step_kXY = static_cast<int>(pow((rhoXY / hXY), 2.0 / 3.0) + 0.5);
          }
          //calc tangent vector
          oneStepSearch(1, step_kXY, X, Y, Z, Xindex, Yindex, Zindex, xyPlaneX, xyPlaneY, isD, "xy");
        }
        /* yz plane */
        if (((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) && !(geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm)) {
          //calc the radius of circumscribed circle
          oneStepSearch(1, step_kYZ, X, Y, Z, Xindex, Yindex, Zindex, yzPlaneY, yzPlaneZ, isD, "yz");
          a = sqrt(pow(((Y - yzPlaneY[0]) * hY) / 2.0, 2) + pow(((Z - yzPlaneZ[0]) * hZ) / 2.0, 2));
          b = sqrt(pow(((Y - yzPlaneY[1]) * hY) / 2.0, 2) + pow(((Z - yzPlaneZ[1]) * hZ) / 2.0, 2));
          c = sqrt(pow(((yzPlaneY[0] - yzPlaneY[1]) * hY) / 2.0, 2) + pow(((yzPlaneZ[0] - yzPlaneZ[1]) * hZ) / 2.0, 2));
          //calc step_k
          if ((a + b + c) == 0 || (-a + b + c) == 0 || (a - b + c) == 0 || (a + b - c) == 0) {
            step_kYZ = 1;
          } else {
            rhoYZ = min(max_radiusYZ, (a * b * c) / sqrt((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c)));
            step_kYZ = static_cast<int>(pow((rhoYZ / hYZ), 2.0 / 3.0) + 0.5);
          }
          //calc tangent vector
          oneStepSearch(1, step_kYZ, X, Y, Z, Xindex, Yindex, Zindex, yzPlaneY, yzPlaneZ, isD, "yz");
        }
        /* xz plane */
        if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) && !(geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {
          //calc the radius of circumscribed circle
          oneStepSearch(1, step_kXZ, X, Y, Z, Xindex, Yindex, Zindex, xzPlaneX, xzPlaneZ, isD, "xz");
          a = sqrt(pow(((X - xzPlaneX[0]) * hX) / 2.0, 2) + pow(((Z - xzPlaneZ[0]) * hZ) / 2.0, 2));
          b = sqrt(pow(((X - xzPlaneX[1]) * hX) / 2.0, 2) + pow(((Z - xzPlaneZ[1]) * hZ) / 2.0, 2));
          c = sqrt(pow(((xzPlaneX[0] - xzPlaneX[1]) * hX) / 2.0, 2) + pow(((xzPlaneZ[0] - xzPlaneZ[1]) * hZ) / 2.0, 2));
          //calc step_k
          if ((a + b + c) == 0 || (-a + b + c) == 0 || (a - b + c) == 0 || (a + b - c) == 0) {
            step_kXZ = 1;
          } else {
            rhoXZ = min(max_radiusXZ, (a * b * c) / sqrt((a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c)));
            step_kXZ = static_cast<int>(pow((rhoXZ / hXZ), 2.0 / 3.0) + 0.5);
          }
          //calc tangent vector
          oneStepSearch(1, step_kXZ, X, Y, Z, Xindex, Yindex, Zindex, xzPlaneX, xzPlaneZ, isD, "xz");
        }

        //set normal unit vector
        //calc extended product
        if (geoInfo->bType[index].isBofXp || geoInfo->bType[index].isBofXm) {//xy * xz
          X1 = xyPlaneX[0] - xyPlaneX[1];
          Y1 = xyPlaneY[0] - xyPlaneY[1];
          Z1 = 0.0;
          X2 = xzPlaneX[0] - xzPlaneX[1];
          Y2 = 0.0;
          Z2 = xzPlaneZ[0] - xzPlaneZ[1];
        }
        if (geoInfo->bType[index].isBofYp || geoInfo->bType[index].isBofYm) {//xy * yz
          X1 = xyPlaneX[0] - xyPlaneX[1];
          Y1 = xyPlaneY[0] - xyPlaneY[1];
          Z1 = 0.0;
          X2 = 0.0;
          Y2 = yzPlaneY[0] - yzPlaneY[1];
          Z2 = yzPlaneZ[0] - yzPlaneZ[1];
        }
        if (geoInfo->bType[index].isBofZp || geoInfo->bType[index].isBofZm) {//xz * yz
          X1 = xzPlaneX[0] - xzPlaneX[1];
          Y1 = 0.0;
          Z1 = xzPlaneZ[0] - xzPlaneZ[1];
          X2 = 0.0;
          Y2 = yzPlaneY[0] - yzPlaneY[1];
          Z2 = yzPlaneZ[0] - yzPlaneZ[1];
        }
        //あとの方が優先なのです
        nuVec[index].nx = Y1 * Z2 - Z1 * Y2;
        nuVec[index].ny = Z1 * X2 - X1 * Z2;
        nuVec[index].nz = X1 * Y2 - Y1 * X2;
        len = sqrt(pow(nuVec[index].nx, 2) + pow(nuVec[index].ny, 2) + pow(nuVec[index].nz, 2));
        nuVec[index].nx /= len;
        nuVec[index].ny /= len;
        nuVec[index].nz /= len;
      }
    }
  }
  return nuVec;
}

/*
  typedef enum _preDirection {
  N = 0, NE, E, SE, S, SW, W, NE
  }preDirection;
*/

void stepSearch(int l, int preD, int step_count, int step_k, int X, int Y, int Z, int Xindex, int Yindex, int Zindex, int *horComponent, int *verComponent, int *isD, std::string plane)
{
  if (step_count == step_k) return;
  int Nindex1 = 0, Nindex2 = 0;
  int Sindex1 = 0, Sindex2 = 0;
  int Eindex1 = 0, Eindex2 = 0;
  int Windex1 = 0, Windex2 = 0;
  int NEindex = 0, NWindex = 0, SEindex = 0, SWindex= 0;
  int hor = 0, ver = 0;
  int indexMax = Zindex * Yindex * Xindex;
  int indexMin = -1;

  if (plane == "xy") {
    hor = X;
    ver = Y;
    Nindex1 = Z * Yindex * Xindex + (Y + 1) * Xindex + X;
    Nindex2 = Z * Yindex * Xindex + (Y + 2) * Xindex + X;
    NEindex = Z * Yindex * Xindex + (Y + 1) * Xindex + (X + 1);
    Eindex1 = Z * Yindex * Xindex + Y * Xindex + (X + 1);
    Eindex2 = Z * Yindex * Xindex + Y * Xindex + (X + 2);
    SEindex = Z * Yindex * Xindex + (Y - 1) * Xindex + (X + 1);
    Sindex1 = Z * Yindex * Xindex + (Y - 1) * Xindex + X;
    Sindex2 = Z * Yindex * Xindex + (Y - 2) * Xindex + X;
    SWindex = Z * Yindex * Xindex + (Y - 1) * Xindex + (X - 1);
    Windex1 = Z * Yindex * Xindex + Y * Xindex + (X - 1);
    Windex2 = Z * Yindex * Xindex + Y * Xindex + (X - 2);
    NWindex = Z * Yindex * Xindex + (Y + 1) * Xindex + (X - 1);
  } else if (plane == "yz") {
    hor = Y;
    ver = Z;
    Nindex1 = (Z + 1) * Yindex * Xindex + Y * Xindex + X;
    Nindex2 = (Z + 2) * Yindex * Xindex + Y * Xindex + X;
    NEindex = (Z + 1) * Yindex * Xindex + (Y + 1) * Xindex + X;
    Eindex1 = Z * Yindex * Xindex + (Y + 1) * Xindex + X;
    Eindex2 = Z * Yindex * Xindex + (Y + 2) * Xindex + X;
    SEindex = (Z - 1) * Yindex * Xindex + (Y + 1) * Xindex + X;
    Sindex1 = (Z - 1) * Yindex * Xindex + Y * Xindex + X;
    Sindex2 = (Z - 2) * Yindex * Xindex + Y * Xindex + X;
    SWindex = (Z - 1) * Yindex * Xindex + (Y - 1) * Xindex + X;
    Windex1 = Z * Yindex * Xindex + (Y - 1) * Xindex + X;
    Windex2 = Z * Yindex * Xindex + (Y - 2) * Xindex + X;
    NWindex = (Z + 1) * Yindex * Xindex + (Y - 1) * Xindex + X;
  } else if (plane == "xz") {
    hor = X;
    ver = Z;
    Nindex1 = (Z + 1) * Yindex * Xindex + Y * Xindex + X;
    Nindex2 = (Z + 2) * Yindex * Xindex + Y * Xindex + X;
    NEindex = (Z + 1) * Yindex * Xindex + Y * Xindex + (X + 1);
    Eindex1 = Z * Yindex * Xindex + Y * Xindex + (X + 1);
    Eindex2 = Z * Yindex * Xindex + Y * Xindex + (X + 2);
    SEindex = (Z - 1) * Yindex * Xindex + Y * Xindex + (X + 1);
    Sindex1 = (Z - 1) * Yindex * Xindex + Y * Xindex + X;
    Sindex2 = (Z - 2) * Yindex * Xindex + Y * Xindex + X;
    SWindex = (Z - 1) * Yindex * Xindex + Y * Xindex + (X - 1);
    Windex1 = Z * Yindex * Xindex + Y * Xindex + (X - 1);
    Windex2 = Z * Yindex * Xindex + Y * Xindex + (X - 2);
    NWindex = (Z + 1) * Yindex * Xindex + Y * Xindex + (X - 1);
  }

  if (indexMax > Nindex2 && isD[Nindex2] == 1 && isD[Nindex1] == 2 && preD != S) {//north
    horComponent[l] = hor;
    verComponent[l] = ver + 2;
    if (plane == "xy") stepSearch(l, N, ++step_count, step_k, hor, ver + 2, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "yz") stepSearch(l, N, ++step_count, step_k, X, hor, ver + 2, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "xz") stepSearch(l, N, ++step_count, step_k, hor, Y, ver + 2, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
  } else if (indexMax > NEindex && isD[NEindex] == 1 && preD != SW) {//northeast
    horComponent[l] = hor + 1;
    verComponent[l] = ver + 1;
    if (plane == "xy") stepSearch(l, NE, ++step_count, step_k, hor + 1, ver + 1, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "yz") stepSearch(l, NE, ++step_count, step_k, X, hor + 1, ver + 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "xz") stepSearch(l, NE, ++step_count, step_k, hor + 1, Y, ver + 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
  } else if (indexMax > Eindex2 && isD[Eindex2] == 1 && isD[Eindex1] == 2 && preD != W) {//east
    horComponent[l] = hor + 2;
    verComponent[l] = ver;
    if (plane == "xy") stepSearch(l, E, ++step_count, step_k, hor + 2, ver, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "yz") stepSearch(l, E, ++step_count, step_k, X, hor + 2, ver, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "xz") stepSearch(l, E, ++step_count, step_k, hor + 2, Y, ver, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
  } else if (indexMax > SEindex && indexMin < SEindex && isD[SEindex] == 1 && preD != NW) {//southeast
    horComponent[l] = hor + 1;
    verComponent[l] = ver - 1;
    if (plane == "xy") stepSearch(l, SE, ++step_count, step_k, hor + 1, ver - 1, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "yz") stepSearch(l, SE, ++step_count, step_k, X, hor + 1, ver - 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "xz") stepSearch(l, SE, ++step_count, step_k, hor + 1, Y, ver - 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
  } else if (indexMin < Sindex2 && isD[Sindex2] == 1 && isD[Sindex1] == 2 && preD != N) {//south
    horComponent[l] = hor;
    verComponent[l] = ver - 2;
    if (plane == "xy") stepSearch(l, S, ++step_count, step_k, hor, ver - 2, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "yz")	stepSearch(l, S, ++step_count, step_k, X, hor, ver - 2, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "xz") stepSearch(l, S, ++step_count, step_k, hor, Y, ver - 2, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
  } else if (indexMin < SWindex && isD[SWindex] == 1 && preD != NE) {//southwest
    horComponent[l] = hor - 1;
    verComponent[l] = ver - 1;
    if (plane == "xy") stepSearch(l, SW, ++step_count, step_k, hor - 1, ver - 1, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "yz") stepSearch(l, SW, ++step_count, step_k, X, hor - 1, ver - 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "xz") stepSearch(l, SW, ++step_count, step_k, hor - 1, Y, ver - 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
  } else if (indexMin < Windex2 && isD[Windex2] == 1 && isD[Windex1] == 2 && preD != E) {//west
    horComponent[l] = hor - 2;
    verComponent[l] = ver;
    if (plane == "xy") stepSearch(l, W, ++step_count, step_k, hor - 2, ver, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "yz") stepSearch(l, W, ++step_count, step_k, X, hor - 2, ver, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "xz") stepSearch(l, W, ++step_count, step_k, hor - 2, Y, ver , Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
  } else if (indexMax > NWindex && indexMin < NWindex && isD[NWindex] == 1 && preD != SE) {//northwest
    horComponent[l] = hor - 1;
    verComponent[l] = ver + 1;
    if (plane == "xy") stepSearch(l, NW, ++step_count, step_k, hor - 1, ver + 1, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "yz") stepSearch(l, NW, ++step_count, step_k, X, hor - 1, ver + 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
    else if (plane == "xz") stepSearch(l, NW, ++step_count, step_k, hor - 1, Y, ver + 1 , Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
  }
}

void stepSearch_no_recursive(int l, int preD, int step_count, int step_k, int X, int Y, int Z, int Xindex, int Yindex, int Zindex, int *horComponent, int *verComponent, int *isD, std::string plane)
{
  int Nindex1 = 0, Nindex2 = 0;
  int Sindex1 = 0, Sindex2 = 0;
  int Eindex1 = 0, Eindex2 = 0;
  int Windex1 = 0, Windex2 = 0;
  int NEindex = 0, NWindex = 0, SEindex = 0, SWindex= 0;
  int hor = 0, ver = 0;
  int indexMax = Zindex * Yindex * Xindex;
  int indexMin = -1;
  while (step_count < step_k) {
    if (plane == "xy") {
      hor = X;
      ver = Y;
      Nindex1 = Z * Yindex * Xindex + (Y + 1) * Xindex + X;
      Nindex2 = Z * Yindex * Xindex + (Y + 2) * Xindex + X;
      NEindex = Z * Yindex * Xindex + (Y + 1) * Xindex + (X + 1);
      Eindex1 = Z * Yindex * Xindex + Y * Xindex + (X + 1);
      Eindex2 = Z * Yindex * Xindex + Y * Xindex + (X + 2);
      SEindex = Z * Yindex * Xindex + (Y - 1) * Xindex + (X + 1);
      Sindex1 = Z * Yindex * Xindex + (Y - 1) * Xindex + X;
      Sindex2 = Z * Yindex * Xindex + (Y - 2) * Xindex + X;
      SWindex = Z * Yindex * Xindex + (Y - 1) * Xindex + (X - 1);
      Windex1 = Z * Yindex * Xindex + Y * Xindex + (X - 1);
      Windex2 = Z * Yindex * Xindex + Y * Xindex + (X - 2);
      NWindex = Z * Yindex * Xindex + (Y + 1) * Xindex + (X - 1);
    } else if (plane == "yz") {
      hor = Y;
      ver = Z;
      Nindex1 = (Z + 1) * Yindex * Xindex + Y * Xindex + X;
      Nindex2 = (Z + 2) * Yindex * Xindex + Y * Xindex + X;
      NEindex = (Z + 1) * Yindex * Xindex + (Y + 1) * Xindex + X;
      Eindex1 = Z * Yindex * Xindex + (Y + 1) * Xindex + X;
      Eindex2 = Z * Yindex * Xindex + (Y + 2) * Xindex + X;
      SEindex = (Z - 1) * Yindex * Xindex + (Y + 1) * Xindex + X;
      Sindex1 = (Z - 1) * Yindex * Xindex + Y * Xindex + X;
      Sindex2 = (Z - 2) * Yindex * Xindex + Y * Xindex + X;
      SWindex = (Z - 1) * Yindex * Xindex + (Y - 1) * Xindex + X;
      Windex1 = Z * Yindex * Xindex + (Y - 1) * Xindex + X;
      Windex2 = Z * Yindex * Xindex + (Y - 2) * Xindex + X;
      NWindex = (Z + 1) * Yindex * Xindex + (Y - 1) * Xindex + X;
    } else if (plane == "xz") {
      hor = X;
      ver = Z;
      Nindex1 = (Z + 1) * Yindex * Xindex + Y * Xindex + X;
      Nindex2 = (Z + 2) * Yindex * Xindex + Y * Xindex + X;
      NEindex = (Z + 1) * Yindex * Xindex + Y * Xindex + (X + 1);
      Eindex1 = Z * Yindex * Xindex + Y * Xindex + (X + 1);
      Eindex2 = Z * Yindex * Xindex + Y * Xindex + (X + 2);
      SEindex = (Z - 1) * Yindex * Xindex + Y * Xindex + (X + 1);
      Sindex1 = (Z - 1) * Yindex * Xindex + Y * Xindex + X;
      Sindex2 = (Z - 2) * Yindex * Xindex + Y * Xindex + X;
      SWindex = (Z - 1) * Yindex * Xindex + Y * Xindex + (X - 1);
      Windex1 = Z * Yindex * Xindex + Y * Xindex + (X - 1);
      Windex2 = Z * Yindex * Xindex + Y * Xindex + (X - 2);
      NWindex = (Z + 1) * Yindex * Xindex + Y * Xindex + (X - 1);
    }
    if (indexMax > Nindex2 && isD[Nindex2] == 1 && isD[Nindex1] == 2 && preD != S) {//north
      horComponent[l] = hor;
      verComponent[l] = ver + 2;
      ++step_count;
      preD = N;
      if (plane == "xy") {
        X = hor;
        Y = ver + 2;
      }
      else if (plane == "yz"){
        Y = hor;
        Z = ver + 2;
      }
      else if (plane == "xz") {
        X = hor;
        Z = ver + 2;
      }
    }
    else if (indexMax > NEindex && isD[NEindex] == 1 && preD != SW) {//northeast
      horComponent[l] = hor + 1;
      verComponent[l] = ver + 1;
      ++step_count;
      preD = NE;
      if (plane == "xy") {
        X = hor + 1;
        Y = ver + 1;
      }
      else if (plane == "yz") {
        Y = hor + 1;
        Z = ver + 1;
      }
      else if (plane == "xz") {
        X = hor + 1;
        Z = ver + 1;
      }
    }
    else if (indexMax > Eindex2 && isD[Eindex2] == 1 && isD[Eindex1] == 2 && preD != W) {//east
      horComponent[l] = hor + 2;
      verComponent[l] = ver;
      ++step_count;
      preD = E;
      if (plane == "xy") {
        X = hor + 2;
        Y = ver;
      }
      else if (plane == "yz") {
        Y = hor + 2;
        Z = ver;
      }
      else if (plane == "xz") {
        X = hor + 2;
        Z = ver;
      }
    }
    else if (indexMax > SEindex && indexMin < SEindex && isD[SEindex] == 1 && preD != NW) {//southeast
      horComponent[l] = hor + 1;
      verComponent[l] = ver - 1;
      ++step_count;
      preD = SE;
      if (plane == "xy") {
        X = hor + 1;
        Y = ver - 1;
      }
      else if (plane == "yz") {
        Y = hor + 1;
        Z = ver - 1;
      }
      else if (plane == "xz") {
        X = hor + 1;
        Z = ver - 1;
      }
    }
    else if (indexMin < Sindex2 && isD[Sindex2] == 1 && isD[Sindex1] == 2 && preD != N) {//south
      horComponent[l] = hor;
      verComponent[l] = ver - 2;
      ++step_count;
      preD = S;
      if (plane == "xy") {
        X = hor;
        Y = ver - 2;
      }
      else if (plane == "yz")	{
        Y = hor;
        Z = ver - 2;
      }
      else if (plane == "xz") {
        X = hor;
        Z = ver - 2;
      }
    }
    else if (indexMin < SWindex && isD[SWindex] == 1 && preD != NE) {//southwest
      horComponent[l] = hor - 1;
      verComponent[l] = ver - 1;
      ++step_count;
      preD = SW;
      if (plane == "xy") {
        X = hor - 1;
        Y = ver - 1;
      }
      else if (plane == "yz") {
        Y = hor - 1;
        Z = ver - 1;
      }
      else if (plane == "xz") {
        X = hor - 1;
        Z = ver - 1;
      }
    }
    else if (indexMin < Windex2 && isD[Windex2] == 1 && isD[Windex1] == 2 && preD != E) {//west
      horComponent[l] = hor - 2;
      verComponent[l] = ver;
      ++step_count;
      preD = W;
      if (plane == "xy") {
        X = hor - 2;
        Y = ver;
      }
      else if (plane == "yz") {
        Y = hor - 2;
        Z = ver;
      }
      else if (plane == "xz") {
        X = hor - 2;
        Z = ver;
      }
    }
    else if (indexMax > NWindex && indexMin < NWindex && isD[NWindex] == 1 && preD != SE) {//northwest
      horComponent[l] = hor - 1;
      verComponent[l] = ver + 1;
      ++step_count;
      preD = NW;
      if (plane == "xy") {
        X = hor - 1;
        Y = ver + 1;
      }
      else if (plane == "yz") {
        Y = hor - 1;
        Z = ver + 1;
      }
      else if (plane == "xz") {
        X = hor - 1;
        Z = ver + 1;
      }
    }
    else {
      cout << "oops!" << flush;
      return;
    }
  }
}

void oneStepSearch(int step_count, int step_k, int X, int Y, int Z, int Xindex, int Yindex, int Zindex, int *horComponent, int *verComponent, int *isD, string plane)
{
  int Nindex1 = 0, Nindex2 = 0;
  int Sindex1 = 0, Sindex2 = 0;
  int Eindex1 = 0, Eindex2 = 0;
  int Windex1 = 0, Windex2 = 0;
  int NEindex = 0, NWindex = 0, SEindex = 0, SWindex= 0;
  int hor = 0, ver = 0;
  int i = 0;// j is unused, so I delete it. by mashimo
  int indexMax = Zindex * Yindex * Xindex;
  int indexMin = -1;

  if (plane == "xy") {
    hor = X;
    ver = Y;
    Nindex1 = Z * Yindex * Xindex + (Y + 1) * Xindex + X;
    Nindex2 = Z * Yindex * Xindex + (Y + 2) * Xindex + X;
    NEindex = Z * Yindex * Xindex + (Y + 1) * Xindex + (X + 1);
    Eindex1 = Z * Yindex * Xindex + Y * Xindex + (X + 1);
    Eindex2 = Z * Yindex * Xindex + Y * Xindex + (X + 2);
    SEindex = Z * Yindex * Xindex + (Y - 1) * Xindex + (X + 1);
    Sindex1 = Z * Yindex * Xindex + (Y - 1) * Xindex + X;
    Sindex2 = Z * Yindex * Xindex + (Y - 2) * Xindex + X;
    SWindex = Z * Yindex * Xindex + (Y - 1) * Xindex + (X - 1);
    Windex1 = Z * Yindex * Xindex + Y * Xindex + (X - 1);
    Windex2 = Z * Yindex * Xindex + Y * Xindex + (X - 2);
    NWindex = Z * Yindex * Xindex + (Y + 1) * Xindex + (X - 1);
  } else if (plane == "yz") {
    hor = Y;
    ver = Z;
    Nindex1 = (Z + 1) * Yindex * Xindex + Y * Xindex + X;
    Nindex2 = (Z + 2) * Yindex * Xindex + Y * Xindex + X;
    NEindex = (Z + 1) * Yindex * Xindex + (Y + 1) * Xindex + X;
    Eindex1 = Z * Yindex * Xindex + (Y + 1) * Xindex + X;
    Eindex2 = Z * Yindex * Xindex + (Y + 2) * Xindex + X;
    SEindex = (Z - 1) * Yindex * Xindex + (Y + 1) * Xindex + X;
    Sindex1 = (Z - 1) * Yindex * Xindex + Y * Xindex + X;
    Sindex2 = (Z - 2) * Yindex * Xindex + Y * Xindex + X;
    SWindex = (Z - 1) * Yindex * Xindex + (Y - 1) * Xindex + X;
    Windex1 = Z * Yindex * Xindex + (Y - 1) * Xindex + X;
    Windex2 = Z * Yindex * Xindex + (Y - 2) * Xindex + X;
    NWindex = (Z + 1) * Yindex * Xindex + (Y - 1) * Xindex + X;
  } else if (plane == "xz") {
    hor = X;
    ver = Z;
    Nindex1 = (Z + 1) * Yindex * Xindex + Y * Xindex + X;
    Nindex2 = (Z + 2) * Yindex * Xindex + Y * Xindex + X;
    NEindex = (Z + 1) * Yindex * Xindex + Y * Xindex + (X + 1);
    Eindex1 = Z * Yindex * Xindex + Y * Xindex + (X + 1);
    Eindex2 = Z * Yindex * Xindex + Y * Xindex + (X + 2);
    SEindex = (Z - 1) * Yindex * Xindex + Y * Xindex + (X + 1);
    Sindex1 = (Z - 1) * Yindex * Xindex + Y * Xindex + X;
    Sindex2 = (Z - 2) * Yindex * Xindex + Y * Xindex + X;
    SWindex = (Z - 1) * Yindex * Xindex + Y * Xindex + (X - 1);
    Windex1 = Z * Yindex * Xindex + Y * Xindex + (X - 1);
    Windex2 = Z * Yindex * Xindex + Y * Xindex + (X - 2);
    NWindex = (Z + 1) * Yindex * Xindex + Y * Xindex + (X - 1);
  }
  while (i < 2) {
    if (indexMax > Nindex2 && isD[Nindex2] == 1 && isD[Nindex1] == 2) {//north
      horComponent[i] = hor;
      verComponent[i] = ver + 2;
      if (plane == "xy") stepSearch_no_recursive(i, N, step_count, step_k, hor, ver + 2, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "yz") stepSearch_no_recursive(i, N, step_count, step_k, X, hor, ver + 2, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "xz") stepSearch_no_recursive(i, N, step_count, step_k, hor, Y, ver + 2, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (i == 0) i++;
      else return;
    }
    if (indexMax > NEindex && isD[NEindex] == 1) {//northeast
      horComponent[i] = hor + 1;
      verComponent[i] = ver + 1;
      if (plane == "xy") stepSearch_no_recursive(i, NE, step_count, step_k, hor + 1, ver + 1, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "yz") stepSearch_no_recursive(i, NE, step_count, step_k, X, hor + 1, ver + 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "xz") stepSearch_no_recursive(i, NE, step_count, step_k, hor + 1, Y, ver + 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (i == 0) i++;
      else return;
    }
    if (indexMax > Eindex2 && isD[Eindex2] == 1 && isD[Eindex1] == 2) {//east
      horComponent[i] = hor + 2;
      verComponent[i] = ver;
      if (plane == "xy") stepSearch_no_recursive(i, E, step_count, step_k, hor + 2, ver, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "yz") stepSearch_no_recursive(i, E, step_count, step_k, X, hor + 2, ver, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "xz") stepSearch_no_recursive(i, E, step_count, step_k, hor + 2, Y, ver, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (i == 0) i++;
      else return;
    }
    if (indexMax > SEindex && indexMin < SEindex && isD[SEindex] == 1) {//southeast
      horComponent[i] = hor + 1;
      verComponent[i] = ver - 1;
      if (plane == "xy") stepSearch_no_recursive(i, SE, step_count, step_k, hor + 1, ver - 1, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "yz") stepSearch_no_recursive(i, SE, step_count, step_k, X, hor + 1, ver - 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "xz") stepSearch_no_recursive(i, SE, step_count, step_k, hor + 1, Y, ver - 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (i == 0) i++;
      else return;
    }
    if (indexMin < Sindex2 && isD[Sindex2] == 1 && isD[Sindex1] == 2) {//south
      horComponent[i] = hor;
      verComponent[i] = ver - 2;
      if (plane == "xy") stepSearch_no_recursive(i, S, step_count, step_k, hor, ver - 2, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "yz") stepSearch_no_recursive(i, S, step_count, step_k, X, hor, ver - 2, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "xz") stepSearch_no_recursive(i, S, step_count, step_k, hor, Y, ver - 2, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (i == 0) i++;
      else return;
    }
    if (indexMin < SWindex && isD[SWindex] == 1) {//southwest
      horComponent[i] = hor - 1;
      verComponent[i] = ver - 1;
      if (plane == "xy") stepSearch_no_recursive(i, SW, step_count, step_k, hor - 1, ver - 1, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "yz") stepSearch_no_recursive(i, SW, step_count, step_k, X, hor - 1, ver - 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      else if (plane == "xz") stepSearch_no_recursive(i, SW, step_count, step_k, hor - 1, Y, ver - 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (i == 0) i++;
      else return;
    }
    if (indexMin < Windex2 && isD[Windex2] == 1 && isD[Windex1] == 2) {//west
      horComponent[i] = hor - 2;
      verComponent[i] = ver;
      if (plane == "xy") stepSearch_no_recursive(i, W, step_count, step_k, hor - 2, ver, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (plane == "yz") stepSearch_no_recursive(i, W, step_count, step_k, X, hor - 2, ver, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (plane == "xz") stepSearch_no_recursive(i, W, step_count, step_k, hor - 2, Y, ver, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (i == 0) i++;
      else return;
    }
    if (indexMax > NWindex && indexMin < NWindex && isD[NWindex] == 1) {//northwest
      horComponent[i] = hor - 1;
      verComponent[i] = ver + 1;
      if (plane == "xy") stepSearch_no_recursive(i, NW, step_count, step_k, hor - 1, ver + 1, Z, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (plane == "yz") stepSearch_no_recursive(i, NW, step_count, step_k, X, hor - 1, ver + 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (plane == "xz") stepSearch_no_recursive(i, NW, step_count, step_k, hor - 1, Y, ver + 1, Xindex, Yindex, Zindex, horComponent, verComponent, isD, plane);
      if (i == 0) i++;
      else return;
    }
    //mashimo
    if (i == 0) {
      i++;
      cout << "oh no!" << flush;
    }
    else return;
  }
}

/*
  search adjacent points, j, of i (step: 1)
  project adjacent points on the tangent plane of i: j_proj
  calc distance between i and j_proj: d_ij
  calc s_j (only 3D)
  average d_ij and d_ji
*/

voronoiInfo* setVoronoiInfo(normalUnitVector *nuVec, variableInfo *xInfo, variableInfo *yInfo, variableInfo *zInfo, std::vector<GeometryInfo*> &geoInfoList, double Xsize, double Ysize, double Zsize, int dimension, int Xindex, int Yindex, int Zindex, unsigned int numOfVolIndexes)
{
  unsigned int i, j, k, l;
  int X, Y, Z, index;
  voronoiInfo *vorI = new voronoiInfo[numOfVolIndexes];
  planeAdjacent *planeAD = new planeAdjacent[numOfVolIndexes];
  InitializeVoronoiInfo(vorI, numOfVolIndexes);
  int *isD = 0;
  GeometryInfo *geoInfo = 0;
  int xyPlaneX[2] = {0}, xyPlaneY[2] = {0};
  int yzPlaneY[2] = {0}, yzPlaneZ[2] = {0};
  int xzPlaneX[2] = {0}, xzPlaneZ[2] = {0};
  double inner_pro = 0.0;
  double d_ij = 0.0, d_ji = 0.0, s_ij = 0.0, s_ji = 0.0;

  for (i = 0; i < geoInfoList.size(); i++) {
    if (!geoInfoList[i]->isVol) {
      geoInfo = geoInfoList[i];
      isD = geoInfo->isDomain;
      for (j = 0; j < geoInfo->domainIndex.size(); j++) {
        index = geoInfo->domainIndex[j];
        Z = index / (Xindex * Yindex);
        Y = (index - Z * Xindex * Yindex) / Xindex;
        X = index - Z * Xindex * Yindex - Y * Xindex;
        //xy plane
        if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {
          //search adjacent points, r_j, of r_i (step: 1)
          oneStepSearch(1, 1, X, Y, Z, Xindex, Yindex, Zindex, xyPlaneX, xyPlaneY, isD, "xy");
          //project adjacent points on the tangent plane of i
          //proj = r_j - N_i (N_i * (r_j - r_i))
          for (l = 0; l < 2; l++) {
            vorI[index].adjacentIndexXY[l] = Z * Xindex * Yindex + xyPlaneY[l] * Xindex + xyPlaneX[l];
            //inner project: N_i * (r_j - r_i)
            if (dimension == 2) {
              inner_pro = nuVec[index].nx * (xInfo->value[vorI[index].adjacentIndexXY[l]] - xInfo->value[index])
                + nuVec[index].ny * (yInfo->value[vorI[index].adjacentIndexXY[l]] - yInfo->value[index]);
            } else if (dimension == 3) {
              inner_pro = nuVec[index].nx * (xInfo->value[vorI[index].adjacentIndexXY[l]] - xInfo->value[index])
                + nuVec[index].ny * (yInfo->value[vorI[index].adjacentIndexXY[l]] - yInfo->value[index])
                + nuVec[index].nz * (zInfo->value[vorI[index].adjacentIndexXY[l]] - zInfo->value[index]);
            }
            planeAD[index].XYcontour[l].nx = xInfo->value[vorI[index].adjacentIndexXY[l]] - nuVec[index].nx * inner_pro;
            planeAD[index].XYcontour[l].ny = yInfo->value[vorI[index].adjacentIndexXY[l]] - nuVec[index].ny * inner_pro;
            if (dimension == 2) {
              vorI[index].diXY[l] = sqrt(pow(xInfo->value[index] - planeAD[index].XYcontour[l].nx, 2) + pow(yInfo->value[index] - planeAD[index].XYcontour[l].ny, 2));
            } else if (dimension == 3) {
              planeAD[index].XYcontour[l].nz = zInfo->value[vorI[index].adjacentIndexXY[l]] - nuVec[index].nz * inner_pro;
              vorI[index].diXY[l] = sqrt(pow(xInfo->value[index] - planeAD[index].XYcontour[l].nx, 2) + pow(yInfo->value[index] - planeAD[index].XYcontour[l].ny, 2) + pow(zInfo->value[index] - planeAD[index].XYcontour[l].nz, 2));
            }

            //cout << (planeAD[index].XYcontour[l].nx - xInfo->value[index]) * nuVec[index].nx + (planeAD[index].XYcontour[l].ny - yInfo->value[index]) * nuVec[index].ny + (planeAD[index].XYcontour[l].nz - zInfo->value[index]) * nuVec[index].nz << endl;
          }
        }//end of xy plane
        if (dimension == 3) {
          //yz plane
          if ((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {
            //search adjacent points, r_j, of r_i (step: 1)
            oneStepSearch(1, 1, X, Y, Z, Xindex, Yindex, Zindex, yzPlaneY, yzPlaneZ, isD, "yz");
            //project adjacent points on the tangent plane of i
            //proj = r_j - N_i (N_i * (r_j - r_i))
            for (l = 0; l < 2; l++) {
              vorI[index].adjacentIndexYZ[l] = yzPlaneZ[l] * Xindex * Yindex + yzPlaneY[l] * Xindex + X;
              inner_pro = nuVec[index].nx * (xInfo->value[vorI[index].adjacentIndexYZ[l]] - xInfo->value[index])
                + nuVec[index].ny * (yInfo->value[vorI[index].adjacentIndexYZ[l]] - yInfo->value[index])
                + nuVec[index].nz * (zInfo->value[vorI[index].adjacentIndexYZ[l]] - zInfo->value[index]);
              planeAD[index].YZcontour[l].nx = xInfo->value[vorI[index].adjacentIndexYZ[l]] - nuVec[index].nx * inner_pro;
              planeAD[index].YZcontour[l].ny = yInfo->value[vorI[index].adjacentIndexYZ[l]] - nuVec[index].ny * inner_pro;
              planeAD[index].YZcontour[l].nz = zInfo->value[vorI[index].adjacentIndexYZ[l]] - nuVec[index].nz * inner_pro;
              vorI[index].diYZ[l] = sqrt(pow(xInfo->value[index] - planeAD[index].YZcontour[l].nx, 2) + pow(yInfo->value[index] - planeAD[index].YZcontour[l].ny, 2) + pow(zInfo->value[index] - planeAD[index].YZcontour[l].nz, 2));
              //cout << (planeAD[index].YZcontour[l].nx - xInfo->value[index]) * nuVec[index].nx + (planeAD[index].YZcontour[l].ny - yInfo->value[index]) * nuVec[index].ny + (planeAD[index].YZcontour[l].nz - zInfo->value[index]) * nuVec[index].nz << endl;
            }
          }//end of yz plane
          //xz plane
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {
            oneStepSearch(1, 1, X, Y, Z, Xindex, Yindex, Zindex, xzPlaneX, xzPlaneZ, isD, "xz");
            //project adjacent points on the tangent plane of i
            //proj = r_j - N_i (N_i * (r_j - r_i))
            for (l = 0; l < 2; l++) {
              vorI[index].adjacentIndexXZ[l] = xzPlaneZ[l] * Xindex * Yindex + Y * Xindex + xzPlaneX[l];
              inner_pro = nuVec[index].nx * (xInfo->value[vorI[index].adjacentIndexXZ[l]] - xInfo->value[index])
                + nuVec[index].ny * (yInfo->value[vorI[index].adjacentIndexXZ[l]] - yInfo->value[index])
                + nuVec[index].nz * (zInfo->value[vorI[index].adjacentIndexXZ[l]] - zInfo->value[index]);
              planeAD[index].XZcontour[l].nx = xInfo->value[vorI[index].adjacentIndexXZ[l]] - nuVec[index].nx * inner_pro;
              planeAD[index].XZcontour[l].ny = yInfo->value[vorI[index].adjacentIndexXZ[l]] - nuVec[index].ny * inner_pro;
              planeAD[index].XZcontour[l].nz = zInfo->value[vorI[index].adjacentIndexXZ[l]] - nuVec[index].nz * inner_pro;
              vorI[index].diXZ[l] = sqrt(pow(xInfo->value[index] - planeAD[index].XZcontour[l].nx, 2) + pow(yInfo->value[index] - planeAD[index].XZcontour[l].ny, 2) + pow(zInfo->value[index] - planeAD[index].XZcontour[l].nz, 2));
              //cout << (planeAD[index].XZcontour[l].nx - xInfo->value[index]) * nuVec[index].nx + (planeAD[index].XZcontour[l].ny - yInfo->value[index]) * nuVec[index].ny + (planeAD[index].XZcontour[l].nz - zInfo->value[index]) * nuVec[index].nz << endl;
            }
          }
        }
      }
      //calc s_ij (only 3D)
      if (dimension == 3) {
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          //平面を回転
          double phi = 0.0, theta = 0.0;
          phi = atan2(nuVec[index].ny, nuVec[index].nx);
          theta = acos(nuVec[index].nz);
          //double nx2 = 0.0, ny2 = 0.0, nz2 = 0.0;
          //nx2 = cos(theta) * (nuVec[index].nx * cos(phi) + nuVec[index].ny * sin(phi)) - nuVec[index].nz * sin(theta);
          //ny2 = -nuVec[index].nx * sin(phi) + nuVec[index].ny * cos(phi);
          //nz2 = sin(theta) * (nuVec[index].nx * cos(phi) + nuVec[index].ny * sin(phi)) + nuVec[index].nz * cos(theta);
          double rotRj_XY_x[2] = {0.0}, rotRj_XY_y[2] = {0.0};
          double rotRj_YZ_x[2] = {0.0}, rotRj_YZ_y[2] = {0.0};
          double rotRj_XZ_x[2] = {0.0}, rotRj_XZ_y[2] = {0.0};

          double rotRi_x = cos(theta) * (xInfo->value[index] * cos(phi) + yInfo->value[index] * sin(phi)) - zInfo->value[index] * sin(theta);
          double rotRi_y = -xInfo->value[index] * sin(phi) + yInfo->value[index] * cos(phi);
          //double rotRi_z = sin(theta) * (xInfo->value[index] * cos(phi) + yInfo->value[index] * sin(phi)) + zInfo->value[index] * cos(theta);// unused variable by mashimo
          //xy-yz
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm))
              && ((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm))) {
            //rotate R_i, ~R_j
            for (l = 0; l < 2; l++) {
              rotRj_XY_x[l] = cos(theta) * (planeAD[index].XYcontour[l].nx * cos(phi) + planeAD[index].XYcontour[l].ny * sin(phi)) - planeAD[index].XYcontour[l].nz * sin(theta);
              rotRj_XY_y[l] = -planeAD[index].XYcontour[l].nx * sin(phi) + planeAD[index].XYcontour[l].ny * cos(phi);

              rotRj_YZ_x[l] = cos(theta) * (planeAD[index].YZcontour[l].nx * cos(phi) + planeAD[index].YZcontour[l].ny * sin(phi)) - planeAD[index].YZcontour[l].nz * sin(theta);
              rotRj_YZ_y[l] = -planeAD[index].YZcontour[l].nx * sin(phi) + planeAD[index].YZcontour[l].ny * cos(phi);
            }
            //cout << (rotRj_XY_z[0] - rotRi_z) << endl;
            //cout << sqrt(pow(rotRj_XY_x[0] - rotRi_x, 2) + pow(rotRj_XY_y[0] - rotRi_y, 2) + pow(rotRj_XY_z[0] - rotRi_z, 2)) << " " << vorI[index].diXY[0] << endl;
            double Px[4] = {0.0}, Py[4] = {0.0};
            double cp_x[2] = {0.0}, cp_y[2] = {0.0};
            double areaP013 = 0.0, areaP123 = 0.0;
            //xy s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_XY_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_XY_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_XY_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_XY_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_YZ_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_YZ_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_YZ_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_YZ_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siXY[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
              //cout << vorI[index].siXY[l] << endl;
            }
            //yz s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_YZ_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_YZ_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_YZ_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_YZ_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_XY_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_XY_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_XY_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_XY_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siYZ[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
              //cout << vorI[index].siYZ[l] << endl;
            }
          }

          //xy-xz
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm))
              && ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm))) {
            for (l = 0; l < 2; l++) {
              rotRj_XY_x[l] = cos(theta) * (planeAD[index].XYcontour[l].nx * cos(phi) + planeAD[index].XYcontour[l].ny * sin(phi)) - planeAD[index].XYcontour[l].nz * sin(theta);
              rotRj_XY_y[l] = -planeAD[index].XYcontour[l].nx * sin(phi) + planeAD[index].XYcontour[l].ny * cos(phi);

              rotRj_XZ_x[l] = cos(theta) * (planeAD[index].XZcontour[l].nx * cos(phi) + planeAD[index].XZcontour[l].ny * sin(phi)) - planeAD[index].XZcontour[l].nz * sin(theta);
              rotRj_XZ_y[l] = -planeAD[index].XZcontour[l].nx * sin(phi) + planeAD[index].XZcontour[l].ny * cos(phi);
            }
            double Px[4] = {0.0}, Py[4] = {0.0};
            double cp_x[2] = {0.0}, cp_y[2] = {0.0};
            double areaP013 = 0.0, areaP123 = 0.0;
            //xy s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_XY_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_XY_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_XY_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_XY_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_XZ_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_XZ_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_XZ_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_XZ_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siXY[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
              //cout << vorI[index].siXY[l] << endl;
            }
            //xz s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_XZ_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_XZ_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_XZ_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_XZ_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_XY_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_XY_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_XY_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_XY_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siXZ[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
              //cout << vorI[index].siXZ[l] << endl;
            }
          }
          //yz-xz
          if (((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm))
              && ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm))) {
            for (l = 0; l < 2; l++) {
              rotRj_YZ_x[l] = cos(theta) * (planeAD[index].YZcontour[l].nx * cos(phi) + planeAD[index].YZcontour[l].ny * sin(phi)) - planeAD[index].YZcontour[l].nz * sin(theta);
              rotRj_YZ_y[l] = -planeAD[index].YZcontour[l].nx * sin(phi) + planeAD[index].YZcontour[l].ny * cos(phi);

              rotRj_XZ_x[l] = cos(theta) * (planeAD[index].XZcontour[l].nx * cos(phi) + planeAD[index].XZcontour[l].ny * sin(phi)) - planeAD[index].XZcontour[l].nz * sin(theta);
              rotRj_XZ_y[l] = -planeAD[index].XZcontour[l].nx * sin(phi) + planeAD[index].XZcontour[l].ny * cos(phi);
            }
            double Px[4] = {0.0}, Py[4] = {0.0};
            double cp_x[2] = {0.0}, cp_y[2] = {0.0};
            double areaP013 = 0.0, areaP123 = 0.0;
            //xy s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_YZ_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_YZ_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_YZ_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_YZ_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_XZ_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_XZ_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_XZ_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_XZ_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siYZ[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
              //cout << vorI[index].siYZ[l] << endl;
            }
            //xz s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_XZ_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_XZ_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_XZ_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_XZ_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_YZ_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_YZ_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_YZ_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_YZ_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siXZ[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
              //cout << vorI[index].siXZ[l] << endl;
            }
          }

          /*
          //xy plane
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {
          if (vorI[index].adjacentIndexYZ[0] != -1 && vorI[index].adjacentIndexYZ[1] != -1) {//adjacent points at yz facet
          vorI[index].siXY[0] = (vorI[index].diYZ[0] + vorI[index].diYZ[1]) / 2.0;
          vorI[index].siXY[1] = vorI[index].siXY[0];
          }
          if (vorI[index].adjacentIndexXZ[0] != -1 && vorI[index].adjacentIndexXZ[1] != -1) {//adjacent points at xz facet
          vorI[index].siXY[0] = (vorI[index].diXZ[0] + vorI[index].diXZ[1]) / 2.0;
          vorI[index].siXY[1] = vorI[index].siXY[0];
          }
          }
          //yz plane
          if ((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {
          if (vorI[index].adjacentIndexXY[0] != -1 && vorI[index].adjacentIndexXY[1] != -1) {//adjacent points at xy facet
          vorI[index].siYZ[0] = (vorI[index].diXY[0] + vorI[index].diXY[1]) / 2.0;
          vorI[index].siYZ[1] = vorI[index].siYZ[0];
          }
          if (vorI[index].adjacentIndexXZ[0] != -1 && vorI[index].adjacentIndexXZ[1] != -1) {//adjacent points at xz facet
          vorI[index].siYZ[0] = (vorI[index].diXZ[0] + vorI[index].diXZ[1]) / 2.0;
          vorI[index].siYZ[1] = vorI[index].siYZ[0];
          }
          }
          //xz plane
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {
          if (vorI[index].adjacentIndexYZ[0] != -1 && vorI[index].adjacentIndexYZ[1] != -1) {//adjacent points at yz facet
          vorI[index].siXZ[0] = (vorI[index].diYZ[0] + vorI[index].diYZ[1]) / 2.0;
          vorI[index].siXZ[1] = vorI[index].siXZ[0];
          }
          if (vorI[index].adjacentIndexXY[0] != -1 && vorI[index].adjacentIndexXY[1] != -1) {//adjacent points at xy facet
          vorI[index].siXZ[0] = (vorI[index].diXY[0] + vorI[index].diXY[1]) / 2.0;
          vorI[index].siXZ[1] = vorI[index].siXZ[0];
          }
          }
          */
        }
      }

      //cocla average of d_ij and s_ij
      for (j = 0; j < geoInfo->domainIndex.size(); j++) {
        index = geoInfo->domainIndex[j];
        //xy plane
        if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {
          for (l = 0; l < 2; l++) {
            if (!vorI[index].isAveXY[l]) {
              d_ij = vorI[index].diXY[l];
              s_ij = vorI[index].siXY[l];
              for (k = 0; k < 2; k++) {
                if (index == vorI[vorI[index].adjacentIndexXY[l]].adjacentIndexXY[k]) {
                  d_ji = vorI[vorI[index].adjacentIndexXY[l]].diXY[k];
                  s_ji = vorI[vorI[index].adjacentIndexXY[l]].siXY[k];
                  break;
                }
              }
              vorI[index].diXY[l] = (d_ij + d_ji) / 2.0;
              vorI[index].siXY[l] = (s_ij + s_ji) / 2.0;
              vorI[vorI[index].adjacentIndexXY[l]].diXY[k] = vorI[index].diXY[l];
              vorI[vorI[index].adjacentIndexXY[l]].siXY[k] = vorI[index].siXY[l];
              vorI[index].isAveXY[l] = true;
              vorI[vorI[index].adjacentIndexXY[l]].isAveXY[k] = true;
            }
          }
        }
        if (dimension == 3) {
          //yz plane
          if ((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {
            for (l = 0; l < 2; l++) {
              if (!vorI[index].isAveYZ[l]) {
                d_ij = vorI[index].diYZ[l];
                s_ij = vorI[index].siYZ[l];
                for (k = 0; k < 2; k++) {
                  if (index == vorI[vorI[index].adjacentIndexYZ[l]].adjacentIndexYZ[k]) {
                    d_ji = vorI[vorI[index].adjacentIndexYZ[l]].diYZ[k];
                    s_ji = vorI[vorI[index].adjacentIndexYZ[l]].siYZ[k];
                    break;
                  }
                }
                vorI[index].diYZ[l] = (d_ij + d_ji) / 2.0;
                vorI[index].siYZ[l] = (s_ij + s_ji) / 2.0;
                vorI[vorI[index].adjacentIndexYZ[l]].diYZ[k] = vorI[index].diYZ[l];
                vorI[vorI[index].adjacentIndexYZ[l]].siYZ[k] = vorI[index].siYZ[l];
                vorI[index].isAveYZ[l] = true;
                vorI[vorI[index].adjacentIndexYZ[l]].isAveYZ[k] = true;
              }
            }
          }
          //xz plane
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {
            for (l = 0; l < 2; l++) {
              if (!vorI[index].isAveXZ[l]) {
                d_ij = vorI[index].diXZ[l];
                s_ij = vorI[index].siXZ[l];
                for (k = 0; k < 2; k++) {
                  if (index == vorI[vorI[index].adjacentIndexXZ[l]].adjacentIndexXZ[k]) {
                    d_ji = vorI[vorI[index].adjacentIndexXZ[l]].diXZ[k];
                    s_ji = vorI[vorI[index].adjacentIndexXZ[l]].siXZ[k];
                    break;
                  }
                }
                vorI[index].diXZ[l] = (d_ij + d_ji) / 2.0;
                vorI[index].siXZ[l] = (s_ij + s_ji) / 2.0;
                vorI[vorI[index].adjacentIndexXZ[l]].diXZ[k] = vorI[index].diXZ[l];
                vorI[vorI[index].adjacentIndexXZ[l]].siXZ[k] = vorI[index].siXZ[l];
                vorI[index].isAveXZ[l] = true;
                vorI[vorI[index].adjacentIndexXZ[l]].isAveXZ[k] = true;
              }
            }
          }
        }
      }
      for (j = 0; j < geoInfo->domainIndex.size(); j++) {
        index = geoInfo->domainIndex[j];
        Z = index / (Xindex * Yindex);
        Y = (index - Z * Xindex * Yindex) / Xindex;
        X = index - Z * Xindex * Yindex - Y * Xindex;
      }

    }
  }

  delete[] planeAD;
  return vorI;
}

voronoiInfo* setVoronoiInfo_modify(normalUnitVector *nuVec, variableInfo *xInfo, variableInfo *yInfo, variableInfo *zInfo, std::vector<GeometryInfo*> &geoInfoList, double Xsize, double Ysize, double Zsize, int dimension, int Xindex, int Yindex, int Zindex, unsigned int numOfVolIndexes)
{
  unsigned int i, j, k, l;
  int X, Y, Z, index;
  voronoiInfo *vorI = new voronoiInfo[numOfVolIndexes];
  planeAdjacent *planeAD = new planeAdjacent[numOfVolIndexes];
  InitializeVoronoiInfo(vorI, numOfVolIndexes);
  int *isD = 0;
  GeometryInfo *geoInfo = 0;
  int xyPlaneX[2] = {0}, xyPlaneY[2] = {0};
  int yzPlaneY[2] = {0}, yzPlaneZ[2] = {0};
  int xzPlaneX[2] = {0}, xzPlaneZ[2] = {0};
  double inner_pro = 0.0;
  double d_ij = 0.0, d_ji = 0.0, s_ij = 0.0, s_ji = 0.0;
  /*
  int Xdiv = (Xindex + 1) / 2;
  int Ydiv = (Yindex + 1) / 2;
  int Zdiv = (Zindex + 1) / 2;
  double deltaX = 0.0, deltaY = 0.0, deltaZ = 0.0;
  deltaX = Xsize / (double)(Xdiv - 1);
  deltaY = Ysize / (double)(Ydiv - 1);
  if (dimension >= 3) deltaZ = Zsize / (double)(Zdiv - 1);
  */

  if (dimension == 2) {
    for (i = 0; i < geoInfoList.size(); i++) {
      if (!geoInfoList[i]->isVol) {
        geoInfo = geoInfoList[i];
        isD = geoInfo->isDomain;
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          Z = index / (Xindex * Yindex);
          Y = (index - Z * Xindex * Yindex) / Xindex;
          X = index - Z * Xindex * Yindex - Y * Xindex;
          //xy plane
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {
            //search adjacent points, r_j, of r_i (step: 1)
            oneStepSearch(1, 1, X, Y, Z, Xindex, Yindex, Zindex, xyPlaneX, xyPlaneY, isD, "xy");
            //project adjacent points on the tangent plane of i
            //proj = r_j - N_i (N_i * (r_j - r_i))
            for (l = 0; l < 2; l++) {
              vorI[index].adjacentIndexXY[l] = Z * Xindex * Yindex + xyPlaneY[l] * Xindex + xyPlaneX[l];
              //inner project: N_i * (r_j - r_i)
              inner_pro = nuVec[index].nx * (xInfo->value[vorI[index].adjacentIndexXY[l]] - xInfo->value[index])
                + nuVec[index].ny * (yInfo->value[vorI[index].adjacentIndexXY[l]] - yInfo->value[index]);
              planeAD[index].XYcontour[l].nx = xInfo->value[vorI[index].adjacentIndexXY[l]] - nuVec[index].nx * inner_pro;
              planeAD[index].XYcontour[l].ny = yInfo->value[vorI[index].adjacentIndexXY[l]] - nuVec[index].ny * inner_pro;
              vorI[index].diXY[l] = sqrt(pow(xInfo->value[index] - planeAD[index].XYcontour[l].nx, 2) + pow(yInfo->value[index] - planeAD[index].XYcontour[l].ny, 2));
            }
          }//end of xy plane
        }

        //cocla average of d_ij and s_ij
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          //xy plane
          if ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {
            for (l = 0; l < 2; l++) {
              if (!vorI[index].isAveXY[l]) {
                d_ij = vorI[index].diXY[l];
                s_ij = vorI[index].siXY[l];
                for (k = 0; k < 2; k++) {
                  if (index == vorI[vorI[index].adjacentIndexXY[l]].adjacentIndexXY[k]) {
                    d_ji = vorI[vorI[index].adjacentIndexXY[l]].diXY[k];
                    s_ji = vorI[vorI[index].adjacentIndexXY[l]].siXY[k];
                    break;
                  }
                }
                vorI[index].diXY[l] = (d_ij + d_ji) / 2.0;
                vorI[index].siXY[l] = (s_ij + s_ji) / 2.0;
                vorI[vorI[index].adjacentIndexXY[l]].diXY[k] = vorI[index].diXY[l];
                vorI[vorI[index].adjacentIndexXY[l]].siXY[k] = vorI[index].siXY[l];
                vorI[index].isAveXY[l] = true;
                vorI[vorI[index].adjacentIndexXY[l]].isAveXY[k] = true;
              }
            }
          }
        }
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          Z = index / (Xindex * Yindex);
          Y = (index - Z * Xindex * Yindex) / Xindex;
          X = index - Z * Xindex * Yindex - Y * Xindex;
        }
      }
    }
  }
  else if (dimension == 3) {
    for (i = 0; i < geoInfoList.size(); i++) {
      if (!geoInfoList[i]->isVol) {
        geoInfo = geoInfoList[i];
        isD = geoInfo->isDomain;
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          Z = index / (Xindex * Yindex);
          Y = (index - Z * Xindex * Yindex) / Xindex;
          X = index - Z * Xindex * Yindex - Y * Xindex;
          //xy plane
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) && !(geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {
            //search adjacent points, r_j, of r_i (step: 1)
            oneStepSearch(1, 1, X, Y, Z, Xindex, Yindex, Zindex, xyPlaneX, xyPlaneY, isD, "xy");
            //project adjacent points on the tangent plane of i
            //proj = r_j - N_i (N_i * (r_j - r_i))
            for (l = 0; l < 2; l++) {
              vorI[index].adjacentIndexXY[l] = Z * Xindex * Yindex + xyPlaneY[l] * Xindex + xyPlaneX[l];
              //inner project: N_i * (r_j - r_i)
              inner_pro = nuVec[index].nx * (xInfo->value[vorI[index].adjacentIndexXY[l]] - xInfo->value[index])
                + nuVec[index].ny * (yInfo->value[vorI[index].adjacentIndexXY[l]] - yInfo->value[index])
                + nuVec[index].nz * (zInfo->value[vorI[index].adjacentIndexXY[l]] - zInfo->value[index]);
              planeAD[index].XYcontour[l].nx = xInfo->value[vorI[index].adjacentIndexXY[l]] - nuVec[index].nx * inner_pro;
              planeAD[index].XYcontour[l].ny = yInfo->value[vorI[index].adjacentIndexXY[l]] - nuVec[index].ny * inner_pro;
              planeAD[index].XYcontour[l].nz = zInfo->value[vorI[index].adjacentIndexXY[l]] - nuVec[index].nz * inner_pro;
                vorI[index].diXY[l] = sqrt(pow(xInfo->value[index] - planeAD[index].XYcontour[l].nx, 2) + pow(yInfo->value[index] - planeAD[index].XYcontour[l].ny, 2) + pow(zInfo->value[index] - planeAD[index].XYcontour[l].nz, 2));
            }
          }//end of xy plane
          //yz plane
          if (((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) && !(geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm)) {
            //search adjacent points, r_j, of r_i (step: 1)
            oneStepSearch(1, 1, X, Y, Z, Xindex, Yindex, Zindex, yzPlaneY, yzPlaneZ, isD, "yz");
            //project adjacent points on the tangent plane of i
            //proj = r_j - N_i (N_i * (r_j - r_i))
            for (l = 0; l < 2; l++) {
              vorI[index].adjacentIndexYZ[l] = yzPlaneZ[l] * Xindex * Yindex + yzPlaneY[l] * Xindex + X;
              inner_pro = nuVec[index].nx * (xInfo->value[vorI[index].adjacentIndexYZ[l]] - xInfo->value[index])
                + nuVec[index].ny * (yInfo->value[vorI[index].adjacentIndexYZ[l]] - yInfo->value[index])
                + nuVec[index].nz * (zInfo->value[vorI[index].adjacentIndexYZ[l]] - zInfo->value[index]);
              planeAD[index].YZcontour[l].nx = xInfo->value[vorI[index].adjacentIndexYZ[l]] - nuVec[index].nx * inner_pro;
              planeAD[index].YZcontour[l].ny = yInfo->value[vorI[index].adjacentIndexYZ[l]] - nuVec[index].ny * inner_pro;
              planeAD[index].YZcontour[l].nz = zInfo->value[vorI[index].adjacentIndexYZ[l]] - nuVec[index].nz * inner_pro;
              vorI[index].diYZ[l] = sqrt(pow(xInfo->value[index] - planeAD[index].YZcontour[l].nx, 2) + pow(yInfo->value[index] - planeAD[index].YZcontour[l].ny, 2) + pow(zInfo->value[index] - planeAD[index].YZcontour[l].nz, 2));
            }
          }//end of yz plane
          //xz plane
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) && !(geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {
            oneStepSearch(1, 1, X, Y, Z, Xindex, Yindex, Zindex, xzPlaneX, xzPlaneZ, isD, "xz");
            //project adjacent points on the tangent plane of i
            //proj = r_j - N_i (N_i * (r_j - r_i))
            for (l = 0; l < 2; l++) {
              vorI[index].adjacentIndexXZ[l] = xzPlaneZ[l] * Xindex * Yindex + Y * Xindex + xzPlaneX[l];
              inner_pro = nuVec[index].nx * (xInfo->value[vorI[index].adjacentIndexXZ[l]] - xInfo->value[index])
                + nuVec[index].ny * (yInfo->value[vorI[index].adjacentIndexXZ[l]] - yInfo->value[index])
                + nuVec[index].nz * (zInfo->value[vorI[index].adjacentIndexXZ[l]] - zInfo->value[index]);
              planeAD[index].XZcontour[l].nx = xInfo->value[vorI[index].adjacentIndexXZ[l]] - nuVec[index].nx * inner_pro;
              planeAD[index].XZcontour[l].ny = yInfo->value[vorI[index].adjacentIndexXZ[l]] - nuVec[index].ny * inner_pro;
              planeAD[index].XZcontour[l].nz = zInfo->value[vorI[index].adjacentIndexXZ[l]] - nuVec[index].nz * inner_pro;
              vorI[index].diXZ[l] = sqrt(pow(xInfo->value[index] - planeAD[index].XZcontour[l].nx, 2) + pow(yInfo->value[index] - planeAD[index].XZcontour[l].ny, 2) + pow(zInfo->value[index] - planeAD[index].XZcontour[l].nz, 2));
            }
          }
        }
        //calc s_ij (only 3D)
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          //平面を回転
          double phi = 0.0, theta = 0.0;
          phi = atan2(nuVec[index].ny, nuVec[index].nx);
          theta = acos(nuVec[index].nz);
          double rotRj_XY_x[2] = {0.0}, rotRj_XY_y[2] = {0.0};
          double rotRj_YZ_x[2] = {0.0}, rotRj_YZ_y[2] = {0.0};
          double rotRj_XZ_x[2] = {0.0}, rotRj_XZ_y[2] = {0.0};

          double rotRi_x = cos(theta) * (xInfo->value[index] * cos(phi) + yInfo->value[index] * sin(phi)) - zInfo->value[index] * sin(theta);
          double rotRi_y = -xInfo->value[index] * sin(phi) + yInfo->value[index] * cos(phi);
          //xy-yz
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm))
              && ((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm))) {
            //rotate R_i, ~R_j
            for (l = 0; l < 2; l++) {
              rotRj_XY_x[l] = cos(theta) * (planeAD[index].XYcontour[l].nx * cos(phi) + planeAD[index].XYcontour[l].ny * sin(phi)) - planeAD[index].XYcontour[l].nz * sin(theta);
              rotRj_XY_y[l] = -planeAD[index].XYcontour[l].nx * sin(phi) + planeAD[index].XYcontour[l].ny * cos(phi);

              rotRj_YZ_x[l] = cos(theta) * (planeAD[index].YZcontour[l].nx * cos(phi) + planeAD[index].YZcontour[l].ny * sin(phi)) - planeAD[index].YZcontour[l].nz * sin(theta);
              rotRj_YZ_y[l] = -planeAD[index].YZcontour[l].nx * sin(phi) + planeAD[index].YZcontour[l].ny * cos(phi);
            }
            double Px[4] = {0.0}, Py[4] = {0.0};
            double cp_x[2] = {0.0}, cp_y[2] = {0.0};
            double areaP013 = 0.0, areaP123 = 0.0;
            //xy s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_XY_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_XY_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_XY_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_XY_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_YZ_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_YZ_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_YZ_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_YZ_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siXY[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
            }
            //yz s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_YZ_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_YZ_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_YZ_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_YZ_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_XY_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_XY_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_XY_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_XY_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siYZ[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
            }
          }
          //xy-xz
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm))
              && ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm))) {
            for (l = 0; l < 2; l++) {
              rotRj_XY_x[l] = cos(theta) * (planeAD[index].XYcontour[l].nx * cos(phi) + planeAD[index].XYcontour[l].ny * sin(phi)) - planeAD[index].XYcontour[l].nz * sin(theta);
              rotRj_XY_y[l] = -planeAD[index].XYcontour[l].nx * sin(phi) + planeAD[index].XYcontour[l].ny * cos(phi);

              rotRj_XZ_x[l] = cos(theta) * (planeAD[index].XZcontour[l].nx * cos(phi) + planeAD[index].XZcontour[l].ny * sin(phi)) - planeAD[index].XZcontour[l].nz * sin(theta);
              rotRj_XZ_y[l] = -planeAD[index].XZcontour[l].nx * sin(phi) + planeAD[index].XZcontour[l].ny * cos(phi);
            }
            double Px[4] = {0.0}, Py[4] = {0.0};
            double cp_x[2] = {0.0}, cp_y[2] = {0.0};
            double areaP013 = 0.0, areaP123 = 0.0;
            //xy s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_XY_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_XY_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_XY_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_XY_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_XZ_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_XZ_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_XZ_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_XZ_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siXY[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
            }
            //xz s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_XZ_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_XZ_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_XZ_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_XZ_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_XY_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_XY_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_XY_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_XY_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siXZ[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
            }
          }
          //yz-xz
          if (((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm))
              && ((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm))) {
            for (l = 0; l < 2; l++) {
              rotRj_YZ_x[l] = cos(theta) * (planeAD[index].YZcontour[l].nx * cos(phi) + planeAD[index].YZcontour[l].ny * sin(phi)) - planeAD[index].YZcontour[l].nz * sin(theta);
              rotRj_YZ_y[l] = -planeAD[index].YZcontour[l].nx * sin(phi) + planeAD[index].YZcontour[l].ny * cos(phi);

              rotRj_XZ_x[l] = cos(theta) * (planeAD[index].XZcontour[l].nx * cos(phi) + planeAD[index].XZcontour[l].ny * sin(phi)) - planeAD[index].XZcontour[l].nz * sin(theta);
              rotRj_XZ_y[l] = -planeAD[index].XZcontour[l].nx * sin(phi) + planeAD[index].XZcontour[l].ny * cos(phi);
            }
            double Px[4] = {0.0}, Py[4] = {0.0};
            double cp_x[2] = {0.0}, cp_y[2] = {0.0};
            double areaP013 = 0.0, areaP123 = 0.0;
            //xy s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_YZ_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_YZ_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_YZ_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_YZ_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_XZ_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_XZ_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_XZ_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_XZ_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siYZ[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
            }
            //xz s_ij
            for (l = 0; l < 2; l++) {//xy
              Px[0] = (rotRj_XZ_x[l] + rotRi_x) / 2.0;
              Py[0] = (rotRj_XZ_y[l] + rotRi_y) / 2.0;
              Px[2] = -(rotRj_XZ_y[l] - rotRi_y) + Px[0];
              Py[2] = (rotRj_XZ_x[l] - rotRi_x) + Py[0];
              for (k = 0; k < 2; k++) {//yz
                Px[1] = (rotRj_YZ_x[k] + rotRi_x) / 2.0;
                Py[1] = (rotRj_YZ_y[k] + rotRi_y) / 2.0;
                Px[3] = -(rotRj_YZ_y[k] - rotRi_y) + Px[1];
                Py[3] = (rotRj_YZ_x[k] - rotRi_x) + Py[1];
                areaP013 = ((Px[3] - Px[1]) * (Py[0] - Py[1]) - (Py[3] - Py[1]) * (Px[0] - Px[1])) / 2.0;
                areaP123 = ((Px[3] - Px[1]) * (Py[1] - Py[2]) - (Py[3] - Py[1]) * (Px[1] - Px[2])) / 2.0;
                cp_x[k] = Px[0] + ((Px[2] - Px[0]) * areaP013) / (areaP013 + areaP123);
                cp_y[k] = Py[0] + ((Py[2] - Py[0]) * areaP013) / (areaP013 + areaP123);
              }
              vorI[index].siXZ[l] = sqrt(pow(cp_x[0] - cp_x[1], 2) + pow(cp_y[0] - cp_y[1], 2));
            }
          }
        }

        //cocla average of d_ij and s_ij
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          //xy plane
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) && !(geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) {
            for (l = 0; l < 2; l++) {
              if (!vorI[index].isAveXY[l]) {
                d_ij = vorI[index].diXY[l];
                s_ij = vorI[index].siXY[l];
                for (k = 0; k < 2; k++) {
                  if (index == vorI[vorI[index].adjacentIndexXY[l]].adjacentIndexXY[k]) {
                    d_ji = vorI[vorI[index].adjacentIndexXY[l]].diXY[k];
                    s_ji = vorI[vorI[index].adjacentIndexXY[l]].siXY[k];
                    break;
                  }
                }
                vorI[index].diXY[l] = (d_ij + d_ji) / 2.0;
                vorI[index].siXY[l] = (s_ij + s_ji) / 2.0;
                vorI[vorI[index].adjacentIndexXY[l]].diXY[k] = vorI[index].diXY[l];
                vorI[vorI[index].adjacentIndexXY[l]].siXY[k] = vorI[index].siXY[l];
                vorI[index].isAveXY[l] = true;
                vorI[vorI[index].adjacentIndexXY[l]].isAveXY[k] = true;
              }
            }
          }
          //yz plane
          if (((geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) && !(geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm)) {
            for (l = 0; l < 2; l++) {
              if (!vorI[index].isAveYZ[l]) {
                d_ij = vorI[index].diYZ[l];
                s_ij = vorI[index].siYZ[l];
                for (k = 0; k < 2; k++) {
                  if (index == vorI[vorI[index].adjacentIndexYZ[l]].adjacentIndexYZ[k]) {
                    d_ji = vorI[vorI[index].adjacentIndexYZ[l]].diYZ[k];
                    s_ji = vorI[vorI[index].adjacentIndexYZ[l]].siYZ[k];
                    break;
                  }
                }
                vorI[index].diYZ[l] = (d_ij + d_ji) / 2.0;
                vorI[index].siYZ[l] = (s_ij + s_ji) / 2.0;
                vorI[vorI[index].adjacentIndexYZ[l]].diYZ[k] = vorI[index].diYZ[l];
                vorI[vorI[index].adjacentIndexYZ[l]].siYZ[k] = vorI[index].siYZ[l];
                vorI[index].isAveYZ[l] = true;
                vorI[vorI[index].adjacentIndexYZ[l]].isAveYZ[k] = true;
              }
            }
          }
          //xz plane
          if (((geoInfo->bType[index].isBofXp && geoInfo->bType[index].isBofXm) || (geoInfo->bType[index].isBofZp && geoInfo->bType[index].isBofZm)) && !(geoInfo->bType[index].isBofYp && geoInfo->bType[index].isBofYm)) {
            for (l = 0; l < 2; l++) {
              if (!vorI[index].isAveXZ[l]) {
                d_ij = vorI[index].diXZ[l];
                s_ij = vorI[index].siXZ[l];
                for (k = 0; k < 2; k++) {
                  if (index == vorI[vorI[index].adjacentIndexXZ[l]].adjacentIndexXZ[k]) {
                    d_ji = vorI[vorI[index].adjacentIndexXZ[l]].diXZ[k];
                    s_ji = vorI[vorI[index].adjacentIndexXZ[l]].siXZ[k];
                    break;
                  }
                }
                vorI[index].diXZ[l] = (d_ij + d_ji) / 2.0;
                vorI[index].siXZ[l] = (s_ij + s_ji) / 2.0;
                vorI[vorI[index].adjacentIndexXZ[l]].diXZ[k] = vorI[index].diXZ[l];
                vorI[vorI[index].adjacentIndexXZ[l]].siXZ[k] = vorI[index].siXZ[l];
                vorI[index].isAveXZ[l] = true;
                vorI[vorI[index].adjacentIndexXZ[l]].isAveXZ[k] = true;
              }
            }
          }
        }
        for (j = 0; j < geoInfo->domainIndex.size(); j++) {
          index = geoInfo->domainIndex[j];
          Z = index / (Xindex * Yindex);
          Y = (index - Z * Xindex * Yindex) / Xindex;
          X = index - Z * Xindex * Yindex - Y * Xindex;
        }
      }
    }
  }
  delete[] planeAD;
  return vorI;
}
