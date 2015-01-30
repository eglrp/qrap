 /*
 *    Q-Rap Project
 *
 *    Version     : 0.1
 *    Date        : 2012/04/24
 *    License     : GNU GPLv3
 *    File        : cTrainAntPattern.cpp
 *    Copyright   : (c) University of Pretoria
 *    Author      : Magdaleen Ballot (magdaleen.ballot@up.ac.za)
 *    Description : This class does the analysis of the measurements in the 
 *			the database
 *
 **************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; See the GNU General Public License for      *
 *   more details                                                          *
 *                                                                         *
 ************************************************************************* */


#include "cTrainAntPattern.h"
#include <random>


//*********************************************************************
cTrainAntPattern::cTrainAntPattern() // default constructor
{
	
	double kS, kI;
	mUnits = dBm;
	string setting;
	setting = gDb.GetSetting("kFactorServer");
	if(setting!="")
		kS=atof(setting.c_str());
	else kS = 1.25;
	setting = gDb.GetSetting("kFactorInt");
	if(setting!="")
		kI=atof(setting.c_str());
	else kI = 1.25;
	mkFactor = min((kI+2*kS)/3.0,1.33);

	setting = gDb.GetSetting("PlotResolution");
	if(setting!="")
	{
		mPlotResolution = atof(setting.c_str());
	}
	else mPlotResolution = 30;

	mPlotResolution = 30;

	setting = gDb.GetSetting("UseClutter");
	if (setting=="true")
		mUseClutter = true;
	else mUseClutter = false;

	mUseClutter = true;

	mClutterSource = atoi(gDb.GetSetting("ClutterSource").c_str());
	cout << "mClutterSource = " << mClutterSource << endl;
	if (mUseClutter)
		cout << "cPosEstimation constructor1:  Using Clutter " <<  endl;
	else cout << "cPosEstimation constructor1:  NOT Using Clutter " <<  endl;
	if (mUseClutter)
		mUseClutter = mClutter.SetRasterFileRules(mClutterSource);
	if (mUseClutter)
		cout << "cPosEstimation constructor2:  Using Clutter " <<  endl;
	else cout << "cPosEstimation constructor2:  NOT Using Clutter " <<  endl;
	if (mUseClutter)
		mClutterClassGroup = mClutter.GetClutterClassGroup();
	if (mUseClutter)
		cout << "cPosEstimation constructor3:  Using Clutter " <<  endl;
	else cout << "cPosEstimation constructor3:  NOT Using Clutter " <<  endl;
	mUseClutter = (mUseClutter)&&(mClutterClassGroup>0)&&(mClutterClassGroup<9000);
	if (mUseClutter) mClutterCount = new unsigned[mPathLoss.mClutter.mNumber];
	else mClutterCount = new unsigned[2];
	if (mUseClutter) mClutter.SetSampleMethod(1);
	if (mUseClutter)
		cout << "cPosEstimation constructor4:  Using Clutter " <<  endl;
	else cout << "cPosEstimation constructor4:  NOT Using Clutter " <<  endl;

	mDEMsource = atoi(gDb.GetSetting("DEMsource").c_str());
	cout << "mDEMsource = " << mDEMsource << endl;
	mDEM.SetRasterFileRules(mDEMsource);
	mDEM.SetSampleMethod(2); 

}

//*********************************************************************
cTrainAntPattern::~cTrainAntPattern() // destructor
{
	unsigned i,j;
	cout << "In cTrainAntPattern::~cTrainAntPattern() " << endl;

	for (i=0; i<mNumCells; i++)
	{
		cout << "In cTrainAntPattern::~cTrainAntPattern():  Cell=" << i << endl;
		for (j=0; j<mCells[i].sNumInputs ;j++)
		{
			delete [] mCells[i].sInputTrain[j];
			delete [] mCells[i].sInputTest[j];
		}
		for (j=0; j<mCells[i].sNumOutputs ;j++)
		{
			delete [] mCells[i].sOutputTrain[j];
			delete [] mCells[i].sOutputTest[j];	
		}
		delete [] mCells[i].sInputTrain;
		delete [] mCells[i].sOutputTrain;	
		delete [] mCells[i].sInputTest;
		delete [] mCells[i].sOutputTest;	
		mCells[i].sMeasTrain.clear();
		mCells[i].sMeasTest.clear();
		cout << "In cTrainAntPattern::~cTrainAntPattern():  Cell=" << i << "	done" << endl;
	}
	mCells.clear();
	cout << "In cTrainAntPattern::~cTrainAntPattern(): mCells cleared " << endl;
}


//*********************************************************************
bool cTrainAntPattern::LoadMeasurements(vPoints Points,
					unsigned MeasType, 
					unsigned MeasSource,
					unsigned PosSource,
					unsigned Technology)
{

	if (Points.size() < 2)
	{
		cout << "cTrainAntPattern::LoadMeasurements   Not enough points to define area. " << endl;
		return false;
	}

	pqxx::result r;
	double Lat, Lon, mNorth, mSouth, mEast, mWest;
	char *text= new char[33];
	cGeoP NorthWestCorner,SouthEastCorner; 
	Points[0].Get(mNorth,mEast);
	mSouth = mNorth;
	mWest = mEast;
	string query, areaQuery;
	pqxx::result MeasSelect;
	unsigned i;

	tCellAnt NewCell;
	NewCell.sCI=0;

	double longitude, latitude;
	double Total, Delta, DiffLoss, PathLoss;
	float Tilt;
	string PointString;
	unsigned spacePos;
	unsigned cellid;
	
	areaQuery += " @ ST_GeomFromText('POLYGON((";
	for (i = 0 ; i < Points.size();i++)
   	{
		Points[i].Get(Lat, Lon);
        	mNorth = max(mNorth,Lat);
        	mSouth = min(mSouth,Lat);
        	mEast = max(mEast,Lon);
        	mWest = min(mWest,Lon);
			gcvt(Lon,12,text);
        	areaQuery += text;
        	areaQuery += " ";
			gcvt(Lat,12,text);
        	areaQuery += text;
        	areaQuery += ",";
   	}
   	NorthWestCorner.Set(mNorth,mWest,DEG);
   	SouthEastCorner.Set(mSouth,mEast,DEG);
		cout << "North West corner: " << endl;
		NorthWestCorner.Display();
		cout << "South East corner: " << endl;
		SouthEastCorner.Display();
		Points[0].Get(Lat,Lon);
		gcvt(Lon,12,text);
   	areaQuery += text;
   	areaQuery += " ";
		gcvt(Lat,12,text);
   	areaQuery += text;
   	areaQuery += "))',4326) ";

	query = "ci, ST_AsText(site.location) as siteLocation, radioinstallation.txantennaheight as height, tp ";
	query += "ST_AsText(testpoint.location) as measLocation, measvalue, frequency ";
	query += "from measurement cross join testpoint ";
	query += "cross join cell cross join radioinstallation  cross join site ";
	query += "where tp=testpoint.id  and ci = cell.id";
	query += "and risector = radioinstallation.id and siteid = site.id";
	query += "and testpoint.positionsource < 2 ";
	query += "and testpoint.location";
	query += areaQuery;
	
	if (MeasType>0)
	{
		query += " and meastype=";
		gcvt(MeasType,9,text);
		query += text;
	}
	if (MeasSource>0)
	{
		query += " and measdatasource=";
		gcvt(MeasSource,9,text);
		query += text;
	}
	if (PosSource>0)
	{
		query += " and positionsource=";
		gcvt(PosSource,9,text);
		query += text;
	}
	if (Technology>0)
	{
		query += " and Technology.id=";
		gcvt(Technology,9,text);
		query += text;
	}
	query += "order ci, tp";
	cout << query << endl;

	if (!gDb.PerformRawSql(query))
	{
		string err = "cTrainAntPattern::LoadMeasurements: ";
		err +="Problem with database query to get measurements from selected area! Problem with query: ";
		err += query;
		cout << err << endl;
		QRAP_ERROR(err.c_str());
		delete [] text;
		return false;
	}

	gDb.GetLastResult(r);
	if (r.size() >0)
	{
		tMeasNNAnt NewMeasurement; 

		for (i=0; i<r.size(); i++)
		{
			cellid = atoi(r[i]["ci"].c_str());
			if (cellid != NewCell.sCI)
			{
				if (NewCell.sCI>0)
				{
						if (NewCell.sNumTrain>0)
						{
							NewCell.sMean = Total / (NewCell.sNumTrain +NewCell.sNumTest);
							mCells.push_back(NewCell);
						}
						NewCell.sMeasTest.clear();
						NewCell.sMeasTrain.clear();
				}
				NewCell.sCI = cellid;
				NewCell.sHeight = atof(r[i]["height"].c_str());
				PointString = r[i]["sitelocation"].c_str();
				spacePos = PointString.find_first_of(' ');
				longitude = atof((PointString.substr(6,spacePos).c_str())); 
				latitude = atof((PointString.substr(spacePos,PointString.length()-1)).c_str());
				NewCell.sPosition.Set(latitude,longitude,DEG);
				NewCell.sNumTrain=0;
				NewCell.sNumTest=0;
				Total=0;
			}
				
			PointString = r[i]["measlocation"].c_str();
			spacePos = PointString.find_first_of(' ');
			longitude = atof((PointString.substr(6,spacePos).c_str())); 
			latitude = atof((PointString.substr(spacePos,PointString.length()-1)).c_str());
			NewMeasurement.sLocation.Set(latitude,longitude,DEG);
			NewMeasurement.sCellID = atoi(r[i]["ci"].c_str());
			NewMeasurement.sFrequency = atof(r[i]["frequency"].c_str());
			NewMeasurement.sMeasValue = atof(r[i]["measvalue"].c_str());
		
			mDEM.GetForLink(NewCell.sPosition,	NewMeasurement.sLocation, mPlotResolution, mDEMProfile);
			if (mUseClutter)
			{
				mClutter.GetForLink(NewCell.sPosition,	NewMeasurement.sLocation, mPlotResolution, mClutterProfile);
			}
			mPathLoss.setParameters(mkFactor,NewMeasurement.sFrequency,
						NewCell.sHeight, MOBILEHEIGHT, mUseClutter, mClutterClassGroup);
			PathLoss = mPathLoss.TotPathLoss(mDEMProfile, Tilt, mClutterProfile, DiffLoss);

			if (DiffLoss < 6)
			{
					Delta = PathLoss + NewMeasurement.sMeasValue;
					Total +=Delta;
					if ((double)i/10*10 == floor((double)i/10)*10)
					{
							NewCell.sMeasTest.push_back(NewMeasurement);
							NewCell.sNumTest++;
					}
					else
					{
							NewCell.sMeasTrain.push_back(NewMeasurement);
							NewCell.sNumTrain++;
					}
			}		
		}// end for number of entries

	} // end if query is NOT empty
	else 
	{
		string err ="Empty query: ";
		err += query;
		cout << err << endl;
		QRAP_ERROR(err.c_str());
		delete [] text;
		return false;
	} //end query is empty

	delete [] text;
	cout << "cTrainAntPattern::LoadMeasurement: leaving ... happy ... " << endl << endl;
	return true;
}


//**************************************************************************************************************************
bool cTrainAntPattern::TrainANDSaveANDTest()
{
	FANN::neural_net	ANN;
	FANN::training_data	TrainData;
	FANN::training_data	TestData;
	double PathLoss, Delta;
	double TrainError, TestError;
	double minTrainError = MAXDOUBLE;
	double minTestError = MAXDOUBLE;
	unsigned i,j,k;
	float Tilt;
	double Azimuth,  DiffLoss;
	bool stop = false;
	string query, queryM, queryC, outdir, filename, queryD;
	char * temp;
	temp = new char[33];
	char * cell;
	cell = new char[33];
	char * annid;
	annid = new char[33];
	char * machineid;
	machineid = new char[33];
	gcvt(gDb.globalMachineID,8,machineid);

//	time_t now;
	unsigned newANNid;

	outdir = gDb.GetSetting("OutputDir");
	if(outdir=="") outdir = "Data/Output/";

	query = "SELECT (MAX(id)+1) AS id FROM AntNeuralNet";
	pqxx::result r;
	gDb.PerformRawSql(query);
	gDb.GetLastResult(r);
	newANNid = atoi(r[0]["id"].c_str());

	queryM = "INSERT into AntNeuralNet (id, Lastmodified, machineid, ci, ";
	queryM += "filename) Values (";

	mNumCells = mCells.size();

	for (i=0; i<mNumCells; i++)
	{
		mCells[i].sNumOutputs = 1;
		mCells[i].sNumInputs = 5;
		mCells[i].sNumTrain = mCells[i].sMeasTrain.size();
		mCells[i].sNumTest = mCells[i].sMeasTest.size();

		mCells[i].sInputTrain = new double*[mCells[i].sNumTrain];
		mCells[i].sOutputTrain = new double*[mCells[i].sNumTrain];
		for (j=0; j < mCells[i].sNumTrain; j++)
		{
			mCells[i].sInputTrain[j] = new double[mCells[i].sNumInputs];
			mCells[i].sOutputTrain[j] = new double[mCells[i].sNumOutputs];
		}

		for (j=0; j<mCells[i].sNumTest; j++)
		{

			mDEM.GetForLink(mCells[i].sPosition,	mCells[i].sMeasTrain[j].sLocation, mPlotResolution, mDEMProfile);
			if (mUseClutter)
			{
				mClutter.GetForLink(mCells[i].sPosition,	mCells[i].sMeasTrain[j].sLocation, mPlotResolution, mClutterProfile);
			}
			mPathLoss.setParameters(mkFactor,mCells[i].sMeasTrain[j].sFrequency,
						mCells[i].sHeight, MOBILEHEIGHT, mUseClutter, mClutterClassGroup);
			PathLoss = mPathLoss.TotPathLoss(mDEMProfile, Tilt, mClutterProfile, DiffLoss);
			Azimuth = mCells[i].sPosition.Bearing( mCells[i].sMeasTrain[j].sLocation);

			mCells[i].sInputTrain[j][0] = 1;
			mCells[i].sInputTrain[j][1] = cos(Azimuth*PI/180);
			mCells[i].sInputTrain[j][2] = sin(Azimuth*PI/180);
			mCells[i].sInputTrain[j][3] = cos(Tilt*PI/180);
			mCells[i].sInputTrain[j][4] = sin(Tilt*PI/180);

			Delta = PathLoss + mCells[i].sMeasTrain[j].sMeasValue;
			
			mCells[i].sOutputTrain[j][0]  = (Delta - mCells[i].sMean) / 60;
		}

		mCells[i].sInputTest = new double*[mCells[i].sNumTest];
		mCells[i].sOutputTest = new double*[mCells[i].sNumTest];
		for (j=0; j < mCells[i].sNumTest; j++)
		{
			mCells[i].sInputTest[j] = new double[mCells[i].sNumInputs];
			mCells[i].sOutputTest[j] = new double[mCells[i].sNumOutputs];
		}

		for (j=0; j<mCells[i].sMeasTest.size(); j++)
		{

			mDEM.GetForLink(mCells[i].sPosition,	mCells[i].sMeasTest[j].sLocation, mPlotResolution, mDEMProfile);
			if (mUseClutter)
			{
				mClutter.GetForLink(mCells[i].sPosition,	mCells[i].sMeasTest[j].sLocation, mPlotResolution, mClutterProfile);
			}
			mPathLoss.setParameters(mkFactor,mCells[i].sMeasTest[j].sFrequency,
						mCells[i].sHeight, MOBILEHEIGHT, mUseClutter, mClutterClassGroup);
			PathLoss = mPathLoss.TotPathLoss(mDEMProfile, Tilt, mClutterProfile, DiffLoss);
			Azimuth = mCells[i].sPosition.Bearing(mCells[i].sMeasTest[j].sLocation);

			mCells[i].sInputTest[j][0] = 1;
			mCells[i].sInputTest[j][1] = cos(Azimuth*PI/180);
			mCells[i].sInputTest[j][2] = sin(Azimuth*PI/180);
			mCells[i].sInputTest[j][3] = cos(Tilt*PI/180);
			mCells[i].sInputTest[j][4] = sin(Tilt*PI/180);

			Delta = PathLoss + mCells[i].sMeasTest[j].sMeasValue;
			
			mCells[i].sOutputTest[j][0]  = (Delta - mCells[i].sMean) / 60;
		}
		
		TrainData.set_train_data(mCells[i].sNumTrain, 
					mCells[i].sNumInputs, mCells[i].sInputTrain,
					mCells[i].sNumOutputs, mCells[i].sOutputTrain);

		TestData.set_train_data(mCells[i].sNumTest, 
					mCells[i].sNumInputs, mCells[i].sInputTest,
					mCells[i].sNumOutputs, mCells[i].sOutputTest);

		unsigned HiddenN1 = 3;
		unsigned HiddenN2 = 2;

		ANN.create_standard(4, mCells[i].sNumInputs, 
					HiddenN1 ,HiddenN2, mCells[i].sNumOutputs);
 		ANN.set_train_error_function(FANN::ERRORFUNC_LINEAR);
		ANN.set_train_stop_function(FANN::STOPFUNC_MSE);
		ANN.set_training_algorithm(FANN::TRAIN_QUICKPROP);
		ANN.set_activation_function_hidden(FANN::SIGMOID_SYMMETRIC);
		ANN.set_activation_function_output(FANN::SIGMOID_SYMMETRIC);
		ANN.randomize_weights(-0.50,0.50);

		cout << "saving ANN: Cell = " << mCells[i].sCI << "	i=" << i << endl;
		gcvt(mCells[i].sCI,9,cell);
		filename = outdir;
		filename += "/";
		filename += cell;
		filename += temp;		
//		time (&now);
//		filename += ctime(&now);
		filename += ".ann";

//		ANN.train_on_data(TrainDataAngle, MAXepoch,REPORTInt,ERROR);

		minTestError = MAXDOUBLE;
		minTrainError = MAXDOUBLE;
		stop = false;	
		k=0;
		while ((k<MAXepoch)&&(!stop))
		{
			TrainError = ANN.train_epoch(TrainData);
			TestError = ANN.test_data(TestData);
			if (0==k%REPORTInt)
			{
//				TestError = ANN.test_data(TestData);
//				TestError = ANN.get_MSE();
				cout << "celld = " << mCells[i].sCI << "	k=" << k 
					<< "	TrainErr = " << TrainError 
					<< "	TestErr = " << TestError << endl;
			}
			if ((TrainError <= minTrainError)&&(TestError<=minTestError)&&(k>MAXepoch/4))
			{
				ANN.save(filename);
//				TestError = ANN.test_data(TestData);
//				TestError = ANN.get_MSE();
/*				cout << "cellid = " << mCells[i].sCI << "	k=" << k 
					<< "	TrainErr = " << TrainError 
					<< "	TestErr = " << TestError << endl;
*/				stop = (TestError < ERROR)&&(TrainError < ERROR);
				minTrainError = TrainError;
				minTestError = TestError;
			}
//			else if (TestError>minTestError*1.05) stop = true;
			k++;
			
		} 	

		ANN.destroy();

		query = queryM;
		gcvt(newANNid,9,annid);
		query +=annid;
		query +=",now(),";
		query += machineid;
		query += ",";
		query +=cell;
		query += ",'";
		query += filename;
		query += "');";


		if (!gDb.PerformRawSql(query))
		{
			string err = "Error inserting NeuralNet by running query: ";
			err += query;
			cout << err <<endl; 
			QRAP_WARN(err.c_str());
			return false;
		}

		ANN.save(filename);
		ANN.destroy();

		newANNid++;
		cout << "saved ANN: cell = " << mCells[i].sCI << "	i=" << i << endl;
	}
	delete [] temp;
	delete [] cell;
	delete [] annid;
	delete [] machineid;
	cout << "tata" << endl;
	return true;
}


