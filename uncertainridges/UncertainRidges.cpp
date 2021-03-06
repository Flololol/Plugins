#include "UncertainRidges.h"

vtkStandardNewMacro(UncertainRidges);

UncertainRidges::UncertainRidges() {

    this->extrName = (char *) "Extremum Probability";
    this->SetNumberOfInputPorts(1);
    this->SetNumberOfOutputPorts(2);
    this->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
                                 vtkDataSetAttributes::SCALARS);
}

int UncertainRidges::FillInputPortInformation(int port, vtkInformation *info) {
    
    if (port == 0) {
        info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
        return 1;
    }
    return 0;
}

int UncertainRidges::FillOutputPortInformation(int port, vtkInformation *info) {
    
    if (port == 0) {
        info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkImageData");
    } 
    if (port == 1) {
        info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkImageData");
    }

    return 1;
}

int UncertainRidges::RequestUpdateExtent(vtkInformation *, vtkInformationVector **inputVector, vtkInformationVector *outputVector) {
    
    vtkInformation *outInfo = outputVector->GetInformationObject(0);
    vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);

    if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP())) {
        double upTime = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP());
        double *inTimes = inInfo->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
        int numInTimes = inInfo->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
        inInfo->Set(vtkMultiTimeStepAlgorithm::UPDATE_TIME_STEPS(), inTimes, numInTimes);
        //inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), extent, 6);
    }
    
    return 1;
}

int UncertainRidges::RequestInformation(vtkInformation *, vtkInformationVector **inputVector,
                                                 vtkInformationVector *outputVector) {
    vtkInformation *outInfo = outputVector->GetInformationObject(0);
    outInfo->Remove(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    outInfo->Remove(vtkStreamingDemandDrivenPipeline::TIME_RANGE());
    return 1;
}


int UncertainRidges::RequestData(vtkInformation *, vtkInformationVector **inputVector,
                                          vtkInformationVector *outputVector) {
    
    vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
    vtkSmartPointer<vtkMultiBlockDataSet> input = vtkMultiBlockDataSet::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
    int numFields = input->GetNumberOfBlocks();
    cout << "Calculating ridge criteria" << endl;

    this->data = vtkSmartPointer<vtkImageData>::New();
    data->ShallowCopy(vtkImageData::SafeDownCast(input->GetBlock(0)));

    
    this->bounds = data->GetBounds(); 
    this->spacing = data->GetSpacing();
    //this->spaceMag = vec3mag(spacing);
    this->gridResolution = data->GetDimensions();
    this->arrayLength = gridResolution[0] * gridResolution[1] * gridResolution[2];
    this->offsetY = gridResolution[0];
    this->offsetZ = gridResolution[0] * gridResolution[1];
    int coutStep = int(double(arrayLength) / 100.0);
    if(coutStep == 0) coutStep = 1;
    currentField = int(outputVector->GetInformationObject(0)->Get(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP()));

    gradVecs = vtkSmartPointer<vtkDoubleArray>::New();
    gradVecs->SetName("Gradients");
    gradVecs->SetNumberOfComponents(3);
    gradVecs->SetNumberOfTuples(arrayLength);
    eps1 = vtkSmartPointer<vtkDoubleArray>::New();
    eps1->SetName("eps1");
    eps1->SetNumberOfComponents(3);
    eps1->SetNumberOfTuples(arrayLength);
    eps2 = vtkSmartPointer<vtkDoubleArray>::New();
    eps2->SetName("eps2");
    eps2->SetNumberOfComponents(3);
    eps2->SetNumberOfTuples(arrayLength);
    lambda1 = vtkSmartPointer<vtkDoubleArray>::New();
    lambda1->SetName("lambda1");
    lambda1->SetNumberOfComponents(1);
    lambda1->SetNumberOfTuples(arrayLength);
    lambda2 = vtkSmartPointer<vtkDoubleArray>::New();
    lambda2->SetName("lambda2");
    lambda2->SetNumberOfComponents(1);
    lambda2->SetNumberOfTuples(arrayLength);    
    

    if((bounds[5] - bounds[4]) == 0){
        is2D = true;
        this->spaceMag = (spacing[0] + spacing[1]) / double(2);
    } else {
        is2D = false;
        this->spaceMag = (spacing[0] + spacing[1] + spacing[2]) / double(3);
        eps3 = vtkSmartPointer<vtkDoubleArray>::New();
        eps3->SetName("eps3");
        eps3->SetNumberOfComponents(3);
        eps3->SetNumberOfTuples(arrayLength);
        lambda3 = vtkSmartPointer<vtkDoubleArray>::New();
        lambda3->SetName("lambda3");
        lambda3->SetNumberOfComponents(1);
        lambda3->SetNumberOfTuples(arrayLength);
    }
    
    extrProbability = vtkDoubleArray::New();
    extrProbability->SetNumberOfComponents(1);
    extrProbability->SetNumberOfTuples(arrayLength);
    extrProbability->SetName(this->extrName);

    cellProb = vtkDoubleArray::New();
    cellProb->SetNumberOfComponents(1);
    cellProb->SetNumberOfTuples(data->GetNumberOfCells());
    cellProb->SetName("Cell Probability");

    /* vtkSmartPointer<vtkDoubleArray> sampleCell = vtkSmartPointer<vtkDoubleArray>::New();
    sampleCell->SetNumberOfComponents(3);
    sampleCell->SetNumberOfTuples(arrayLength);
    sampleCell->SetName("Sample Gradients");
    vtkSmartPointer<vtkDoubleArray> sampleEps1 = vtkSmartPointer<vtkDoubleArray>::New();
    sampleEps1->SetNumberOfComponents(3);
    sampleEps1->SetNumberOfTuples(arrayLength);
    sampleEps1->SetName("Sample Eps1");
    vtkSmartPointer<vtkDoubleArray> sampleEps2 = vtkSmartPointer<vtkDoubleArray>::New();
    sampleEps2->SetNumberOfComponents(3);
    sampleEps2->SetNumberOfTuples(arrayLength);
    sampleEps2->SetName("Sample Eps2");
    vec3 zerovec = {0, 0, 0};

    for(int i = 0; i < arrayLength; i++){
        sampleCell->SetTuple(i, zerovec);
        sampleEps1->SetTuple(i, zerovec);
        sampleEps2->SetTuple(i, zerovec);
    } */

    this->gen.seed(42);
    this->beginning = nanoClock::now(); //clock for random seed and calculation time
    //Every step of the algorithm in one pipeline for every sample
    cout << "Extracting features..." << endl;
    int calcMean = 0;
    #pragma omp parallel for
    for(int pointIndex = 0; pointIndex < arrayLength; pointIndex++){

        std::vector<Vector80d> neighborhood = std::vector<Vector80d>(numFields, Vector80d::Zero());
        std::vector<Vector24d> neighborhood2D = std::vector<Vector24d>(numFields, Vector24d::Zero());
        Vector80d meanVector = Vector80d::Zero();
        Vector24d meanVector2D = Vector24d::Zero();
        Matrix80d decomposition = Matrix80d::Zero();
        Matrix24d decomposition2D = Matrix24d::Zero();

        ++calcMean;
        if(calcMean % coutStep == 0){
            std::chrono::duration<double> calcTime = beginning - nanoClock::now();
            int prog = int((double(calcMean) / double(arrayLength))*100);
            double ETR = (double(calcTime.count())/prog) * (100 - prog);
            cout << '\r' << std::flush;
            cout << "Prog:" << prog << "%, ETR:" << ETR << "s";
        }
        
        if(isCloseToEdge(pointIndex)){
            gradVecs->SetTuple3(pointIndex, 0.0, 0.0, 0.0);
            eps1->SetTuple3(pointIndex, 0.0, 0.0, 0.0);
            eps2->SetTuple3(pointIndex, 0.0, 0.0, 0.0);
            lambda1->SetTuple1(pointIndex, 0.0);
            lambda2->SetTuple1(pointIndex, 0.0);
            if(!is2D){
                eps3->SetTuple3(pointIndex, 0.0, 0.0, 0.0);
                lambda3->SetTuple1(pointIndex, 0.0);
            }
            extrProbability->SetTuple1(pointIndex, 0.0);
            continue; // Cell at the edge of the field, do nothing
        }

        std::set<int> indices; //set only contains a single instance of any entitiy
        
        if(is2D){
            //get indices of cell nodes, then adjacent points of every node, then adjacent points of the adjacent points leading to 24 points in a 2D cell
            int nodes[4] = {pointIndex, pointIndex+1, pointIndex+offsetY, pointIndex+offsetY+1};

            for(int i = 0; i < 4; i++){
                int point = nodes[i];
                int adjacentPoints[5] = {point, point+1, point-1, point+offsetY, point-offsetY};

                for(int j = 0; j < 5; j++){
                    int adjPoint = adjacentPoints[j];
                    int adjacentOfAdjacent[5] = {adjPoint, adjPoint+1, adjPoint-1, adjPoint+offsetY, adjPoint-offsetY};

                    for(int k = 0; k < 5; k++){
                        indices.insert(adjacentOfAdjacent[k]);
                    }
                }
            }   
        } else {
            //get indices of cell nodes, then adjacent points of every node, then adjacent points of the adjacent points (Pointception) leading to 80 points in a 3D cell
            int nodes[8] = {pointIndex, pointIndex+1, pointIndex+offsetY, pointIndex+offsetY+1, pointIndex+offsetZ,
                            pointIndex+offsetZ+1, pointIndex+offsetY+offsetZ, pointIndex+offsetY+offsetZ+1};
            
            for (int i = 0; i < 8; i++){
                int point = nodes[i];
                int adjacentPoints[7] = {point, point-1, point+1, point-offsetY, point+offsetY, point-offsetZ, point+offsetZ};

                for(int j = 0; j < 7; j++){
                    int adjPoint = adjacentPoints[j];
                    int adjacentOfAdjacent[7] = {adjPoint, adjPoint-1, adjPoint+1, adjPoint-offsetY, adjPoint+offsetY, adjPoint-offsetZ, adjPoint+offsetZ};
                    for(int k = 0; k < 7; k++){
                        indices.insert(adjacentOfAdjacent[k]);
                    }
                }   
            }
        }
        
        for(int fieldIndex = 0; fieldIndex < numFields; fieldIndex++){
            
            int c = 0;
            for(auto it = indices.begin(); it != indices.end(); it++, c++){
                double transfer;
                int ind = *it;
                assert(ind >= 0);
                assert(ind < arrayLength);
                //Calculating mean vec
                transfer = (vtkImageData::SafeDownCast(input->GetBlock(fieldIndex))->GetPointData()->GetScalars())->GetComponent(ind, 0);
                if(is2D){
                    meanVector2D[c] += transfer;
                    //Summing accumulated field for covariance calculation
                    neighborhood2D[fieldIndex][c] = transfer;
                } else {
                    meanVector[c] += transfer;

                    neighborhood[fieldIndex][c] = transfer;
                }
            }
        }

        if(!calcCertain){

            if(is2D){
                //Covariance calculation and cholesky decomposition/eigenvector calculation for 2D cell
                meanVector2D = meanVector2D / double(numFields);
                Matrix24d covarMat = Matrix24d::Zero();

                for(int fieldIndex = 0; fieldIndex < numFields; fieldIndex++){
                    Vector24d transferVec = neighborhood2D[fieldIndex] - meanVector2D;
                    covarMat += transferVec * transferVec.transpose();
                }

                covarMat = covarMat / double(numFields);

                if(useCholesky){
                    Eigen::LDLT<Matrix24d> cholesky(covarMat);

                    Matrix24d D = Matrix24d::Identity();
                    D.diagonal() = cholesky.vectorD();

                    decomposition2D = cholesky.matrixL() * D;
                } else {
                    Eigen::EigenSolver<Matrix24d> eigenMat(covarMat);

                    Matrix24c evDiag = Matrix24c::Identity();
                    evDiag.diagonal() = eigenMat.eigenvalues();

                    decomposition2D = (eigenMat.eigenvectors() * evDiag).real(); //very small complex parts are possible, taking real part just to be sure
                }
            } else {
                //Covariance calculation and cholesky decomposition/eigenvector calculation for 3D cell
                meanVector = meanVector / double(numFields);
                Matrix80d covarMat = Matrix80d::Zero();
                
                for(int fieldIndex = 0; fieldIndex < numFields; fieldIndex++){
                    Vector80d transferVec = neighborhood[fieldIndex] - meanVector;
                    covarMat += transferVec * transferVec.transpose();
                }

                covarMat = covarMat / double(numFields);

                if(useCholesky){
                    Eigen::LDLT<Matrix80d> cholesky(covarMat);

                    Matrix80d D = Matrix80d::Identity();
                    D.diagonal() = cholesky.vectorD();

                    decomposition = cholesky.matrixL() * D;
                } else {
                    Eigen::EigenSolver<Matrix80d> eigenMat(covarMat, true);

                    Matrix80c evDiag = Matrix80c::Identity();
                    evDiag.diagonal() = eigenMat.eigenvalues();

                    decomposition = (eigenMat.eigenvectors() * evDiag).real(); //very small (e.g. 10^-16) complex parts are possible, taking real part just to be sure
                }
            }
        }
        
        double certainProb = 0.0;
        if(is2D){
            vec2 gradients[4];
            mat2 hessians[4];
            vec2 secGrads[4];
            double EV[2];
            vec2 ev1, ev2;
            computeGradients2D(neighborhood2D[currentField], gradients, hessians, secGrads);
            if(this->useNewMethod){
                certainProb = computeRidgeLine2DTest(gradients, hessians, secGrads);
            } else {
                certainProb = computeRidgeLine2D(gradients, hessians, secGrads);
            }
            gradVecs->SetTuple3(pointIndex, gradients[0][0], gradients[0][1], 0.0);
            mat2eigenvalues(hessians[0], EV);
            std::sort(EV, EV + 2);
            mat2realEigenvector(hessians[0], EV[0], ev1);
            mat2realEigenvector(hessians[0], EV[1], ev2);
            //vec2scal(ev1, EV[0], ev1);
            //vec2scal(ev2, EV[1], ev2);
            eps1->SetTuple3(pointIndex, ev1[0], ev1[1], 0.0);
            eps2->SetTuple3(pointIndex, ev2[0], ev2[1], 0.0);
            lambda1->SetTuple1(pointIndex, EV[0]);
            lambda2->SetTuple1(pointIndex, EV[1]);
        } else {
            vec3 gradients[8];
            mat3 hessians[8];
            vec3 secGrads[8];
            double EV[3];
            vec3 ev1, ev2, ev3;

            if(computeLines){
                computeGradients(neighborhood[currentField], gradients, hessians, secGrads, true);
                certainProb = computeParVectors(gradients, hessians, secGrads);
            } else {
                computeGradients(neighborhood[currentField], gradients, hessians, secGrads);
                if(this->useNewMethod){
                    certainProb = computeRidgeSurfaceTest(gradients, hessians);
                } else {
                    certainProb = computeRidgeSurface(gradients, hessians);
                }
            }

            gradVecs->SetTuple(pointIndex, gradients[0]);
            mat3eigenvalues(hessians[0], EV);
            std::sort(EV, EV + 3);
            mat3realEigenvector(hessians[0], EV[0], ev1);
            mat3realEigenvector(hessians[0], EV[1], ev2);
            mat3realEigenvector(hessians[0], EV[2], ev3);
            //vec3scal(ev1, EV[0], ev1);
            //vec3scal(ev2, EV[1], ev2);
            //vec3scal(ev3, EV[2], ev3);
            eps1->SetTuple(pointIndex, ev1);
            eps2->SetTuple(pointIndex, ev2);
            eps3->SetTuple(pointIndex, ev3);
            lambda1->SetTuple1(pointIndex, EV[0]);
            lambda2->SetTuple1(pointIndex, EV[1]);
            lambda3->SetTuple1(pointIndex, EV[2]);
        }
        if(calcCertain){
            extrProbability->SetTuple1(pointIndex, certainProb);
        } else {
            /* if(useRandomSeed){
                nanoClock::duration d = nanoClock::now() - beginning;
                unsigned seed = d.count();
                this->gen.seed(seed);
            } else {
                this->gen.seed(42);
            } */

            double prob = 0.0;
            for (int sampleIteration = 0; sampleIteration < numSamples; sampleIteration++){
                
                if(is2D){
                    Vector24d normalVec = generateNormalDistributedVec2D();
                    Vector24d sample = decomposition2D * normalVec + meanVector2D;
                    /* if(pointIndex == 10000){
                        //printSample(sample);
                        vec2 gradients[4];
                        mat2 hessians[4];
                        vec2 secGrads[4];
                        vec2 eigenvectors[4][2];
                        computeGradients2D(sample, gradients, hessians, secGrads);
                        sampleCell->SetTuple3(pointIndex, gradients[0][0], gradients[0][1], 0);
                        sampleCell->SetTuple3(pointIndex+1, gradients[1][0], gradients[1][1], 0);
                        sampleCell->SetTuple3(pointIndex+offsetY, gradients[2][0], gradients[2][1], 0);
                        sampleCell->SetTuple3(pointIndex+offsetY+1, gradients[3][0], gradients[3][1], 0);
                        for(int i = 0; i < 4; i++){
                            double eigenvalues[2];
                            mat2eigenvalues(hessians[i], eigenvalues);
                            std::sort(eigenvalues, eigenvalues + 2);
                            mat2realEigenvector(hessians[i], eigenvalues[0], eigenvectors[i][0]);
                            vec2scal(eigenvectors[i][0], eigenvalues[0], eigenvectors[i][0]);
                            mat2realEigenvector(hessians[i], eigenvalues[1], eigenvectors[i][1]);
                            vec2scal(eigenvectors[i][1], eigenvalues[1], eigenvectors[i][1]);
                        }
                        sampleEps1->SetTuple3(pointIndex, eigenvectors[0][0][0], eigenvectors[0][0][1], 0);
                        sampleEps1->SetTuple3(pointIndex+1, eigenvectors[1][0][0], eigenvectors[1][0][1], 0);
                        sampleEps1->SetTuple3(pointIndex+offsetY, eigenvectors[2][0][0], eigenvectors[2][0][1], 0);
                        sampleEps1->SetTuple3(pointIndex+offsetY+1, eigenvectors[3][0][0], eigenvectors[3][0][1], 0);

                        sampleEps2->SetTuple3(pointIndex, eigenvectors[0][1][0], eigenvectors[0][1][1], 0);
                        sampleEps2->SetTuple3(pointIndex+1, eigenvectors[1][1][0], eigenvectors[1][1][1], 0);
                        sampleEps2->SetTuple3(pointIndex+offsetY, eigenvectors[2][1][0], eigenvectors[2][1][1], 0);
                        sampleEps2->SetTuple3(pointIndex+offsetY+1, eigenvectors[3][1][0], eigenvectors[3][1][1], 0);
                    } */
                    
                    prob += computeRidge2D(sample);
                } else {
                    Vector80d normalVec = generateNormalDistributedVec();
                    Vector80d sample = decomposition * normalVec + meanVector;

                    prob += computeRidge(sample);
                }
            }
            double frequency = prob / double(numSamples);
            extrProbability->SetTuple1(pointIndex, frequency);            
        }
    }
    cout << '\r' << "Done.                                          " << endl;

    //Transforming point data to cell data as we essentially have cell data with a dummy layer
    int cellInd = 0;
    int pointInd = 0;
    if(is2D){
        for(int y = 0; y < gridResolution[1]; y++){
            for(int x = 0; x < gridResolution[0]; x++){
                if(((x % (gridResolution[0]-1)) == 0 and (x != 0)) or ((y % (gridResolution[1]-1)) == 0 and (y != 0))){
                    pointInd++;
                    continue;
                } else {
                    cellProb->SetTuple(cellInd, extrProbability->GetTuple(pointInd));
                    cellInd++;
                    pointInd++;  
                }
            }
        }
    } else {
        for(int z = 0; z < gridResolution[2]; z++){
            for(int y = 0; y < gridResolution[1]; y++){
                for(int x = 0; x < gridResolution[0]; x++){
                    if(((x % (gridResolution[0]-1)) == 0 and (x != 0)) or ((y % (gridResolution[1]-1)) == 0 and (y != 0)) or ((z % (gridResolution[2]-1)) == 0 and (z != 0))){
                        pointInd++;
                        continue;
                    } else {
                        cellProb->SetTuple(cellInd, extrProbability->GetTuple(pointInd));
                        cellInd++;
                        pointInd++;
                    }
                }
            }
        }
    }
    vtkSmartPointer<vtkImageData> celldata = vtkSmartPointer<vtkImageData>::New();
    celldata->CopyStructure(data);
    //celldata->GetPointData()->AddArray(extrProbability);
    celldata->GetCellData()->AddArray(cellProb);

    vtkSmartPointer<vtkImageData> grads = vtkSmartPointer<vtkImageData>::New();
    grads->CopyStructure(data);

    grads->GetPointData()->AddArray(gradVecs);
    grads->GetPointData()->AddArray(eps1);
    grads->GetPointData()->AddArray(eps2);
    grads->GetPointData()->AddArray(lambda1);
    grads->GetPointData()->AddArray(lambda2);
    if(!is2D){
        grads->GetPointData()->AddArray(eps3);
        grads->GetPointData()->AddArray(lambda3);
    }
    /* grads->GetPointData()->AddArray(sampleCell);
    grads->GetPointData()->AddArray(sampleEps1);
    grads->GetPointData()->AddArray(sampleEps2); */

    vtkInformation *outInfo0 = outputVector->GetInformationObject(0);
    vtkDataObject *output0 = vtkDataObject::SafeDownCast(outInfo0->Get(vtkDataObject::DATA_OBJECT()));

    vtkInformation *outInfo1 = outputVector->GetInformationObject(1);
    vtkDataObject *output1 = vtkDataObject::SafeDownCast(outInfo1->Get(vtkDataObject::DATA_OBJECT()));

    output0->ShallowCopy(celldata);
    output1->ShallowCopy(grads);
    auto t_end = nanoClock::now();
    std::chrono::duration<double> durationTime = t_end - beginning;

    std::cout << "Uncertain Ridge calculation finished in " << durationTime.count() << "s." << std::endl;

    return 1;
}

bool UncertainRidges::isCloseToEdge(int index){
    
    bool isClose = false;
    //check if index is close to an edge in x direction
    if(((index+3) % gridResolution[0] == 0) or ((index+2) % gridResolution[0] == 0) or (((index+1) % gridResolution[0]) == 0) or 
        ((index % gridResolution[0]) == 0) or (((index-1) % gridResolution[0]) == 0)){
        
        isClose = true;
    }
    //check if index is close to an edge in y direction
    if(((index % offsetZ) < (gridResolution[0] * 2)) or ((index % offsetZ) >= (offsetZ - (gridResolution[0]*3)))){
        isClose = true;
    }
    //check if index is close to an edge in z direction if data is 3D
    if(!is2D){
        if((index < (offsetZ*2)) or (index >= (arrayLength - (offsetZ*3)))){
            isClose = true;
        }
    }

    return isClose;
}

void UncertainRidges::printSample(Vector24d sample){
    cout << "Printing sample" << endl;
    for(int i = 0; i < 24; i++){
        cout << "[" << i << "]:" << sample[i] << endl;
    }
}

Vector80d UncertainRidges::generateNormalDistributedVec(){

    std::uniform_real_distribution<double> randomVecEntry(0.00001, 1.0); 
    
    Vector80d normalVec = Vector80d::Zero();

    for(int pair = 0; pair < 40; pair++){
            
        double u1 = randomVecEntry(gen);
        double u2 = randomVecEntry(gen);
        //Box Muller Transformation
        double z1 = sqrt(-2.0*log(u1))*cos((2*M_PI)*u2);
        double z2 = sqrt(-2.0*log(u1))*sin((2*M_PI)*u2);

        normalVec(pair*2) = z1;
        normalVec((pair*2)+1) = z2;
    }

    return normalVec;
}

Vector24d UncertainRidges::generateNormalDistributedVec2D(){

    std::uniform_real_distribution<double> randomVecEntry(0.00001, 1.0); 
    
    Vector24d normalVec = Vector24d::Zero();

    for(int pair = 0; pair < 12; pair++){
            
        double u1 = randomVecEntry(gen);
        double u2 = randomVecEntry(gen);
        //Box Muller Transformation
        double z1 = sqrt(-2.0*log(u1)) * cos((2*M_PI)*u2);
        double z2 = sqrt(-2.0*log(u1)) * sin((2*M_PI)*u2);

        normalVec(pair*2) = z1;
        normalVec((pair*2)+1) = z2;
    }

    return normalVec;
}

void UncertainRidges::computeGradients(Vector80d sampleVector, vec3 *gradients, mat3 *hessians, vec3 *secGrads, bool calcSec){
    //Takes an 80 dimensional sample vector of the scalar field, splits it up and calculates the first and second gradient via central differences

    //The values in sampleVector are sorted ascending by their indices in the original field, every subarray of firstGradientIndices is a node of the helper cell
    //in the derived scalar field, the values are the indices of x+1,x-1,y+1,y-1,z+1 and z-1 for easy gradient and hessian calculation
    int firstGradientIndices[32][6] = {{8,6,11,4,24,0},{9,7,12,5,25,1},{12,10,14,7,30,2},{13,11,15,8,31,3},{20,18,24,16,43,4},{21,19,25,17,44,5},
                                      {24,22,29,18,47,6},{25,23,30,19,48,7},{26,24,31,20,49,8},{27,25,32,21,50,9},{30,28,34,23,53,10},{31,29,35,24,54,11},
                                      {32,30,36,25,55,12},{33,31,37,26,56,13},{36,34,38,30,59,14},{37,35,39,31,60,15},{44,42,48,40,64,19},{45,43,49,41,65,20},
                                      {48,46,53,42,66,23},{49,47,54,43,67,24},{50,48,55,44,68,25},{51,49,56,45,69,26},{54,52,58,47,70,29},{55,53,59,48,71,30},
                                      {56,54,60,49,72,31},{57,55,61,50,73,32},{60,58,62,54,74,35},{61,59,63,55,75,36},{68,66,71,64,76,48},{69,67,72,65,77,49},
                                      {72,70,74,67,78,54},{73,71,75,68,79,55}}; //all those magic numbers

    vec3 tempGradient[32];

    for(int node = 0; node < 32; node++){
        Vector3d gradientVec = Vector3d::Zero();
        //central differences
        gradientVec.row(0) = (sampleVector.row(firstGradientIndices[node][0]) - sampleVector.row(firstGradientIndices[node][1])) / (2 * spacing[0]);
        gradientVec.row(1) = (sampleVector.row(firstGradientIndices[node][2]) - sampleVector.row(firstGradientIndices[node][3])) / (2 * spacing[1]);
        gradientVec.row(2) = (sampleVector.row(firstGradientIndices[node][4]) - sampleVector.row(firstGradientIndices[node][5])) / (2 * spacing[2]);
        //switch to linalg
        tempGradient[node][0] = gradientVec[0]; tempGradient[node][1] = gradientVec[1]; tempGradient[node][2] = gradientVec[2];
    }
    //here the first index is the actual gradient of the node, followed again by x+1, x-1, y+1, y-1, z+1, z-1
    int secondGradientIndices[8][7] = {{7,8,6,11,4,19,0},{8,9,7,12,5,20,1},{11,12,10,14,7,23,2},{12,13,11,15,8,24,3},
                                      {19,20,18,23,16,28,7},{20,21,19,24,17,29,8},{23,24,22,26,19,30,11},{24,25,23,27,20,31,12}};
    
    for(int vec = 0; vec < 8; vec++){
        vec3 A; vec3 B; vec3 C;
        vec3 Adiv; vec3 Bdiv; vec3 Cdiv;
        mat3 hes;
        vec3sub(tempGradient[secondGradientIndices[vec][1]], tempGradient[secondGradientIndices[vec][2]], A);
        vec3scal(A, (double(1)/(2 * spacing[0])), Adiv);

        vec3sub(tempGradient[secondGradientIndices[vec][3]], tempGradient[secondGradientIndices[vec][4]], B);
        vec3scal(B, (double(1)/(2 * spacing[1])), Bdiv);

        vec3sub(tempGradient[secondGradientIndices[vec][5]], tempGradient[secondGradientIndices[vec][6]], C);
        vec3scal(C, (double(1)/(2 * spacing[2])), Cdiv);

        mat3setcols(hes, Adiv, Bdiv, Cdiv);

        if(calcSec){
            vec3 secG;
            mat3vec(hes, tempGradient[secondGradientIndices[vec][0]], secG);
            vec3copy(secG, secGrads[vec]);
        }
        
        vec3copy(tempGradient[secondGradientIndices[vec][0]], gradients[vec]);
        mat3copy(hes, hessians[vec]);
    }
}

void UncertainRidges::computeGradients2D(Vector24d sampleVector, vec2 *gradients, mat2 *hessians, vec2 *secGrads){
    //Takes a 24 dimensional sample vector of the scalar field, splits it up and calculates the first and second gradient via central differences
    vec2 tempGradient[12];

    //The values in sampleVector are sorted ascending by their indices in the original field, every subarray of firstGradientIndices is a node of the helper cell
    //in the derived scalar field, the values are the indices of x+1, x-1, y+1 and y-1 for easy gradient and hessian calculation
    int firstGradientIndices[12][4] = {{4,2,8,0},{5,3,9,1},{8,6,13,2},{9,7,14,3},{10,8,15,4},{11,9,16,5},
                                      {14,12,18,7},{15,13,19,8},{16,14,20,9},{17,15,21,10},{20,18,22,14},{21,19,23,15}};

    for(int node = 0; node < 12; node++){
        Vector2d gradientVec = Vector2d::Zero();
        //central differences
        gradientVec.row(0) = (sampleVector.row(firstGradientIndices[node][0]) - sampleVector.row(firstGradientIndices[node][1])) / (2 * spacing[0]);
        gradientVec.row(1) = (sampleVector.row(firstGradientIndices[node][2]) - sampleVector.row(firstGradientIndices[node][3])) / (2 * spacing[1]);
        //switch to linalg
        tempGradient[node][0] = gradientVec[0]; tempGradient[node][1] = gradientVec[1];
    }
    //here the first index is the actual gradient of the node, followed again by x+1, x-1, y+1, y-1
    int secondGradientIndices[4][5] = {{3,4,2,7,0},{4,5,3,8,1},{7,8,6,10,3},{8,9,7,11,4}};

    for(int vec = 0; vec < 4; vec++){
        vec2 A; vec2 B;
        vec2 Adiv; vec2 Bdiv;
        vec2 secG;
        mat2 hes;

        vec2sub(tempGradient[secondGradientIndices[vec][1]], tempGradient[secondGradientIndices[vec][2]], A);
        vec2scal(A, (double(1)/(2 * spacing[0])), Adiv);

        vec2sub(tempGradient[secondGradientIndices[vec][3]], tempGradient[secondGradientIndices[vec][4]], B);
        vec2scal(B, (double(1)/(2 * spacing[1])), Bdiv);

        mat2setcols(hes, Adiv, Bdiv);
        mat2vec(hes, tempGradient[secondGradientIndices[vec][0]], secG);

        vec2copy(tempGradient[secondGradientIndices[vec][0]], gradients[vec]);
        mat2copy(hes, hessians[vec]);
        vec2copy(secG, secGrads[vec]);
    }
}

double UncertainRidges::computeParVectors(vec3 *gradients, mat3 *hessians, vec3 *secGrads){
    
    double s[3];
    double t[3];
    
    int numParal = 0;
    int isExtremum = 0;

    //indices of every triangular face of a hexahedron with 0-7 being the indices of the cell nodes, 6 cellfaces with 2 triangles each, 3 nodes per triangle
    int cellfaceIndices[6][2][3] = {{{0,1,3},{0,2,3}},
                                    {{0,1,5},{0,4,5}},
                                    {{0,2,6},{0,4,6}},
                                    {{1,3,5},{3,5,7}},
                                    {{2,3,6},{3,6,7}},
                                    {{4,5,6},{5,6,7}}};

    vec3 faceVel[3];
    vec3 faceAcc[3];
    mat3 faceHes[3];

    for(int cellface = 0; cellface < 6; cellface++){
    
        for(int tria = 0; tria < 2; tria++){
            
            vec3copy(gradients[cellfaceIndices[cellface][tria][0]], faceVel[0]);
            vec3copy(gradients[cellfaceIndices[cellface][tria][1]], faceVel[1]);
            vec3copy(gradients[cellfaceIndices[cellface][tria][2]], faceVel[2]);

            vec3copy(secGrads[cellfaceIndices[cellface][tria][0]], faceAcc[0]);
            vec3copy(secGrads[cellfaceIndices[cellface][tria][1]], faceAcc[1]);
            vec3copy(secGrads[cellfaceIndices[cellface][tria][2]], faceAcc[2]);

            numParal = computeParallelOnCellface(faceVel, faceAcc, s, t); //actual parallel vectors
            
            for(int par = 0; par < numParal; par++){
                bool hasExtremum = false;

                mat3copy(hessians[cellfaceIndices[cellface][tria][0]], faceHes[0]);
                mat3copy(hessians[cellfaceIndices[cellface][tria][1]], faceHes[1]);
                mat3copy(hessians[cellfaceIndices[cellface][tria][2]], faceHes[2]);
                
                hasExtremum = isRidgeOrValley(faceHes, faceVel, s[par], t[par]);

                if(hasExtremum) isExtremum++;
            }
        }
    }

    if(isExtremum > 1){ //at least two faces of the cell contain the extremum
        return 1.0;
    } else {
        return 0.0;
    }
}

int UncertainRidges::computeParallelOnCellface(vec3 *faceVel, vec3 *faceAcc, double *s, double *t){

    //parallel vectors implementation
    vec3 v0, v1, v2;
    vec3copy(faceVel[0], v0);
    vec3copy(faceVel[1], v1);
    vec3copy(faceVel[2], v2);

    vec3 w0, w1, w2;
    vec3copy(faceAcc[0], w0);
    vec3copy(faceAcc[1], w1);
    vec3copy(faceAcc[2], w2);
    
    vec3 v01, v02;
    vec3 w01, w02;
    mat3 V, Vinv;
    mat3 W, Winv;
    mat3 M; //Matrix for eigenvector problem, either W^-1 * V or V^-1 * W

    double eigenvalues[3];
    vec3 eigenvectors[3];

    double detV, detW;
    double absdetV, absdetW, absdetmax;
    double nx, ny, nz;
    double ss, tt;
    int numParal;
    int numEigen;
    int ok ,take;

    // The vectors v0->v1 and v0->v2 span the triangle.
	// The vectors v0,v01,v02 are the columns of the V matrix.

    vec3sub(v1, v0, v01);
    vec3sub(v2, v0, v02);
    vec3sub(w1, w0, w01);
    vec3sub(w2, w0, w02);

    mat3setcols(V, v0, v01, v02);
    mat3setcols(W, w0, w01, w02);

    detW = mat3det(W);
	detV = mat3det(V);

	absdetW = fabs(detW);
	absdetV = fabs(detV);

    //Taking matrix with larger determinant
    take = 0;
    absdetmax = 0.0;

    if (absdetW > absdetmax){
			take = 1;
			absdetmax = absdetW;
	}
    if (absdetV > absdetmax) take = 2;

    switch (take) {
        case 0:
            //Matrices not invertible
            return 0;

        case 1:
            mat3invdet(W, detW, Winv);
            mat3mul(Winv, V, M);
            break;
        case 2:
            mat3invdet(V, detV, Vinv);
            mat3mul(Vinv, W, M);
            break;
    }

    numParal = 0;
    numEigen = mat3eigenvalues(M, eigenvalues);
    
    for (int i = 0; i < numEigen; i++){

        ok = mat3realEigenvector(M, eigenvalues[i], eigenvectors[i]);
        //invert eigenvalues if V got inverted
        if(take == 2) {
            if (eigenvalues[i] == 0.0){
                ok = 0;
            } else eigenvalues[i] = 1.0 / eigenvalues[i];
        }

        if(ok){
            //scale the normed eigenvector (nx,ny,nz) to length (1,s,t)
            nx = eigenvectors[i][0];
            ny = eigenvectors[i][1];
            nz = eigenvectors[i][2];

            if (nx != 0.0){
                //local coords in triangle

                ss = ny / nx;
                tt = nz / nx;

                //check if point is inside the triangle
                if ((ss >= 0) and (tt >= 0) and (ss + tt <= 1)){
                    s[numParal] = ss;
                    t[numParal] = tt;
                    numParal++;
                }

            }
        }
    }
    
    return numParal;
}

bool UncertainRidges::isRidgeOrValley(mat3 *hessians, vec3 *faceVel, double s, double t){

    //calculates if found parallel vectors point fits the chosen extremum
    vec3 velVec;
    mat3 interpolated;
    vec3 cross;
    double PCAeigenvalues[3];
    double eigenvalues[3];
    vec3 eigenvectors[4];
    int extrInd = -1;

    //now need the index of the biggest ev for ridge and smallest for valley since we compute lines
    if(this->extremum == 0){ //ridge
        extrInd = 2;
    } else if(this->extremum == 1){ //valley
        extrInd = 0;
    }

    mat3lerp3(hessians[0], hessians[1], hessians[2], s, t, interpolated);
    vec3lerp3(faceVel[0], faceVel[1], faceVel[2], s, t, velVec);

    int numR = mat3eigenvalues(interpolated, eigenvalues);
    if(numR < 3) return 0;
    std::sort(eigenvalues, eigenvalues + 3);
    mat3realEigenvector(interpolated, eigenvalues[extrInd], eigenvectors[0]);

    for(int i = 0; i < 3; i++){
        double tempEV[3];
        int numRR = mat3eigenvalues(hessians[i], tempEV);
        if(numRR < 3) return 0;
        std::sort(tempEV, tempEV + 3);
        mat3realEigenvector(hessians[i], tempEV[extrInd], eigenvectors[i+1]);
    }

    //PCA ON WHOLE CELL WITH EV OF FOUND INTERPOLMAT, then look for alignment, then criteria
    PCA::computeConsistentNodeValuesByPCA(eigenvectors, 4, PCAeigenvalues);

    if(this->evThresh < 1.0){ //PCA subdomain filter
        double evLimit = PCAeigenvalues[0] * this->evThresh;
        if(PCAeigenvalues[1] > evLimit) return 0;
    }

    vec3nrm(velVec, velVec);
    vec3cross(velVec, eigenvectors[0], cross);
    double mag = vec3mag(cross);
    if(mag < this->crossTol){
        if(this->extremum == 0){ //ridge
            if(eigenvalues[1] < -(this->evMin)) return 1;
        } else if(this->extremum == 1){ //valley
            if(eigenvalues[1] > this->evMin) return 1;
        }
    }

    return 0;
}

bool UncertainRidges::computeRidgeLine2D(vec2 *gradients, mat2 *hessians, vec2 *secGrads){

    int signs[4];
    double PCAeigenvalues[2];
    double dotProducts[4];
    vec2 eigenvectors[4];
    int extrInd = -1;
    int isExtremum = 0;

    if(this->extremum == 0){ //ridge
        extrInd = 0;
    } else if(this->extremum == 1){ //valley
        extrInd = 1;
    } else {
        cout << "No Extremum chosen!" << endl;
        return 0;
    }

    //auto func=[](double i, double j) { return abs(i) > abs(j); };

    for(int i = 0; i < 4; i++){
        double EV[2];
        int numR = mat2eigenvalues(hessians[i], EV);
        if(numR < 2) return 0;
        std::sort(EV, EV + 2); //sorting for eberly
        //std::sort(EV, EV + 2, func); //sorting for lindeberg
        mat2realEigenvector(hessians[i], EV[extrInd], eigenvectors[i]);
    }

    PCA2D::computeConsistentNodeValuesByPCA(eigenvectors, 4, PCAeigenvalues);

    if(this->evThresh < 1.0){ //PCA subdomain filter
        double evLimit = PCAeigenvalues[0] * this->evThresh;
        if(PCAeigenvalues[1] > evLimit) return 0;
    }

    for(int i = 0; i < 4; i++){
        vec2 nrmVec;
        vec2nrm(gradients[i], nrmVec);
        dotProducts[i] = vec2dot(nrmVec, eigenvectors[i]);

        if(dotProducts[i] > 0){
            signs[i] = 1;
        } else if(dotProducts[i] == 0) {
            signs[i] = 0;
        } else if(dotProducts[i] < 0){
            signs[i] = -1;
        }
    }

    int ord[4] = {0,1,3,2};
    for(int i = 0; i < 4; i++){
        if(signs[ord[i]] != signs[ord[(i + 1) % 4]]){
            mat2 ipolMat;
            vec2 ipolVec;
            vec2 ipolEv;
            double ipolEigenvalues[2];
            vec2 ipolEigenvectors[2];

            double t = dotProducts[ord[i]] / (dotProducts[ord[i]] - dotProducts[ord[(i+1) % 4]]);

            vec2lerp(gradients[ord[i]], gradients[ord[(i+1) % 4]], t, ipolVec);
            mat2lerp(hessians[ord[i]], hessians[ord[(i+1) % 4]], t, ipolMat);
            mat2eigenvalues(ipolMat, ipolEigenvalues);

            int ind = -1;
            int coGradInd = -1;

            vec2nrm(ipolVec, ipolVec);
            for(int j = 0; j < 2; j++){
                mat2realEigenvector(ipolMat, ipolEigenvalues[j], ipolEigenvectors[j]);
                double crossP = fabs(vec2cross(ipolVec, eigenvectors[j]));
                if(crossP < this->crossTol) ind = j;
            }
            switch(ind){
                case 0: coGradInd = 1; break;
                case 1: coGradInd = 0; break;
                default: continue;
            }

            if(this->extremum == 0){
                if(ipolEigenvalues[coGradInd] < -(this->evMin)) return 1;
            } else if(this->extremum == 1){
                if(ipolEigenvalues[coGradInd] > this->evMin) return 1;
            }
        }
    }

    //improoved condition for marching cubes but not the desired one
    /* double unitVecs[2][2][2] = {{{1.0, 0.0},{-1.0, 0.0}},{{0.0, 1.0},{0.0, -1.0}}};
    for(int i = 0; i < 4; i++){
        if(signs[ord[i]] != signs[ord[(i + 1) % 4]]){
            mat2 ipolMat;
            vec2 ipolVec;
            vec2 ipolEv;
            double ipolEvalues[2];
            int ind1 = (i < 2) ? 0 : 1;
            int ind2 = (i < 2) ? 1 : 0;

            int signLL = (vec2dot(gradients[ord[i]], unitVecs[i%2][ind1]) >= 0) ? 1 : -1;
            int signUR = (vec2dot(gradients[ord[(i + 1) % 4]], unitVecs[i%2][ind2]) >= 0) ? 1 : -1;
            if (signLL != signUR) continue;

            double t = dotProducts[ord[i]] / (dotProducts[ord[i]] - dotProducts[ord[(i+1) % 4]]);

            vec2lerp(gradients[ord[i]], gradients[ord[(i+1) % 4]], t, ipolVec);
            mat2lerp(hessians[ord[i]], hessians[ord[(i+1) % 4]], t, ipolMat);
            int numR = mat2eigenvalues(ipolMat, ipolEvalues);
            if(numR < 2) continue;
            std::sort(ipolEvalues, ipolEvalues + 2);
            mat2realEigenvector(ipolMat, ipolEvalues[extrInd], ipolEv);

            vec2nrm(ipolVec, ipolVec);
            double dotP = fabs(vec2dot(ipolVec, ipolEv));
            //if(dotP < this->crossTol){
                if(this->extremum == 0){
                    if(signLL == 1 and signUR == 1){
                        if(ipolEvalues[extrInd] < -(this->evMin)) isExtremum++;
                    }
                } else if(this->extremum == 1){
                    if(signLL == -1 and signUR == -1){
                        if(ipolEvalues[extrInd] > this->evMin) isExtremum++;
                    }
                }
            //}
        }
    } */
    if(isExtremum > 1){
        return 1;
    } else {
        return 0;
    }
}

double UncertainRidges::computeRidgeLine2DTest(vec2 *gradients, mat2 *hessians, vec2 *secGrads){
    //returns the percentage of nodes that are close to a ridge
    double eigenvalues[4];
    double PCAeigenvalues[2];
    vec2 eigenvectors[4];
    int isExtremum = 0;
    int extrInd = -1;

    if(this->extremum == 0){ //ridge
        extrInd = 0;
    } else if(this->extremum == 1){ //valley
        extrInd = 1;
    } else {
        cout << "No Extremum chosen!" << endl;
        return 0;
    }

    //auto func=[](double i, double j) { return abs(i) > abs(j); };

    for(int i = 0; i < 4; i++){
        double tempEV[2];
        int numR = mat2eigenvalues(hessians[i], tempEV);
        if(numR < 2) return 0;
        std::sort(tempEV, tempEV + 2); //eberly
        //std::sort(tempEV, tempEV + 2, func); //lindeberg
        eigenvalues[i] = tempEV[extrInd];
        mat2realEigenvector(hessians[i], tempEV[extrInd], eigenvectors[i]);
    }

    PCA2D::computeConsistentNodeValuesByPCA(eigenvectors, 4, PCAeigenvalues);

    if(this->evThresh < 1.0){ //PCA subdomain filter
        double evLimit = PCAeigenvalues[0] * this->evThresh;
        if(PCAeigenvalues[1] > evLimit) return 0;
    }

    for(int i = 0; i < 4; i++){
        /* vec2 nrmVec;
        vec2 nrmEV;
        vec2nrm(gradients[i], nrmVec);
        vec2nrm(eigenvectors[i], nrmEV);

        double dotP = fabs(vec2dot(nrmVec, nrmEV));
        double gradMag = vec2mag(gradients[i]);
        if(gradMag == 0.0) gradMag = 0.001;

        double lookupScale = double(1) / gradMag;
        double evScale = fabs(eigenvalues[i] * (this->spaceMag * lookupScale)); */

        double dotP = fabs(vec3dot(gradients[i], eigenvectors[i]));
        double evScale = fabs(eigenvalues[i] * this->spaceMag);

        if(evScale > dotP){
            if(this->extremum == 0){
                if(eigenvalues[i] < -(this->evMin)) isExtremum++;
            } else if(this->extremum == 1){
                if(eigenvalues[i] > this->evMin) isExtremum++;
            }
        }
    }
    double value = double(isExtremum) / double(4);
    
    return value;
}

double UncertainRidges::computeRidgeSurface(vec3 *gradients, mat3 *hessians){
    //returns 0 if no extremum is found, 1 if the cell contains an extremum
    double eigenvalues[15]; //eigenvalues from marching cubes hessians
    vec3 ipolGradients[15]; //gradients from MC
    vec3 eigenvectors[23]; //8 vectors from the 8 cell nodes + (max) 15 possible vectors from MC, all in one for PCA
    double PCAeigenvalues[3];
    double dotProducts[8];
    int isExtremum = 0;
    int signChanged = 0;
    
    int extrInd = -1;

    if(this->extremum == 0){ //ridge
        extrInd = 0;
    } else if(this->extremum == 1){ //valley
        extrInd = 2;
    } else {
        cout << "No Extremum chosen!" << endl;
        return 0;
    }

    //auto func=[](double i, double j) { return abs(i) > abs(j); };

    for(int i = 0; i < 8; i++){
        double tempEV[3];
        int numR = mat3eigenvalues(hessians[i], tempEV);
        if(numR < 3) return 0;
        std::sort(tempEV, tempEV + 3); //sorting for eberly
        //std::sort(tempEV, tempEV + 3, func); //sorting for lindeberg
        mat3realEigenvector(hessians[i], tempEV[extrInd], eigenvectors[i]);
    }

    PCA::computeConsistentNodeValuesByPCA(eigenvectors, 8, PCAeigenvalues); //maybe remove

    if(evThresh < 1.0){ //PCA subdomain filter
        double evLimit = PCAeigenvalues[0] * evThresh;
        if(PCAeigenvalues[1] > evLimit) return 0;
    }

    std::string binaryInd = "";

    for(int i = 0; i < 8; i++){
        vec3 nrmVec;
        vec3nrm(gradients[i], nrmVec);
        dotProducts[i] = vec3dot(nrmVec, eigenvectors[i]);
        
        if(dotProducts[i] >= 0){ //generating marching cubes index with dot(gradient, eigenvector) = 0 as iso level
            binaryInd.insert(0, "1");
        } else {
            binaryInd.insert(0, "0");
        }
    }

    int mcInd = std::stoi(binaryInd, nullptr, 2); //convert binary marching cubes index to integer

    for(int i = 0; i < 5; i++){
        for(int j = 0; j < 3; j++){

            int node1 = mc_table[mcInd].tria[i][j][0];
            int node2 = mc_table[mcInd].tria[i][j][1];

            if(node1 == node2){
                break; //if condition is met, no more 0 crossings occur in this cell because of design of lookup table
            } else {
                mat3 ipolMat;
                vec3 ipolVec;
                vec3 ipolEvector;
                double ipolEV[3];

                double t = dotProducts[node1] / (dotProducts[node1] - dotProducts[node2]); //interpolation weight

                mat3lerp(hessians[node1], hessians[node2], t, ipolMat);
                int numR = mat3eigenvalues(ipolMat, ipolEV);
                if(numR < 3) continue;

                std::sort(ipolEV, ipolEV + 3);
                eigenvalues[signChanged] = ipolEV[extrInd]; //copy eigenvalue corresponding to chosen extremum

                mat3realEigenvector(ipolMat, ipolEV[extrInd], ipolEvector);
                vec3copy(ipolEvector, eigenvectors[8 + signChanged]); //copy respective eigenvector

                vec3lerp(gradients[node1], gradients[node2], t, ipolVec);
                vec3nrm(ipolVec, ipolVec);
                vec3copy(ipolVec, ipolGradients[signChanged]); //copy respective gradient

                signChanged++;
            }
        }
    }

    for(int i = 0; i < signChanged; i++){

        double dotP = fabs(vec3dot(ipolGradients[i], eigenvectors[8 + i]));
        if(dotP < this->crossTol){

            if(this->extremum == 0){ //ridge
                if(eigenvalues[i] < -(this->evMin)) isExtremum++;
            } else if(this->extremum == 1){ //valley
                if(eigenvalues[i] > this->evMin) isExtremum++;
            }
        }
    }

    if(isExtremum > 1){
        return 1.0;
    } else {
        return 0.0;
    }
}

double UncertainRidges::computeRidgeSurfaceTest(vec3 *gradients, mat3 *hessians){
    //returns the percentage of nodes that are close to a ridge
    double eigenvalues[8];
    vec3 eigenvectors[8];
    double PCAeigenvalues[3];
    int isExtremum = 0;
    int extrInd = -1;

    if(this->extremum == 0){ //ridge
        extrInd = 0;
    } else if(this->extremum == 1){ //valley
        extrInd = 2;
    } else {
        cout << "No Extremum chosen!" << endl;
        return 0;
    }

    //auto func=[](double i, double j) { return fabs(i) > fabs(j); };

    for(int i = 0; i < 8; i++){
        double tempEV[3];
        int numR = mat3eigenvalues(hessians[i], tempEV);
        if(numR < 3) return 0;
        std::sort(tempEV, tempEV + 3); //eberly
        //std::sort(tempEV, tempEV + 3, func); //lindeberg
        eigenvalues[i] = tempEV[extrInd];
        mat3realEigenvector(hessians[i], tempEV[extrInd], eigenvectors[i]);
    }

    PCA::computeConsistentNodeValuesByPCA(eigenvectors, 8, PCAeigenvalues); //orient all eigenvectors consistent

    if(evThresh < 1.0){ //PCA subdomain filter
        double evLimit = PCAeigenvalues[0] * evThresh;
        if(PCAeigenvalues[1] > evLimit) return 0;
    }

    for(int i = 0; i < 8; i++){
        /* vec3 nrmVec;
        vec3 nrmEV;
        vec3nrm(gradients[i], nrmVec);
        vec3nrm(eigenvectors[i], nrmEV);
        double dotP = fabs(vec3dot(nrmVec, nrmEV));
        double gradMag = vec3mag(gradients[i]);
        if(gradMag == 0.0) gradMag = 0.0001;
        double lookupScale = double(1) / gradMag;
        double evScale = fabs(eigenvalues[i] * this->spaceMag * lookupScale);*/
        
        double dotP = fabs(vec3dot(gradients[i], eigenvectors[i]));
        double evScale = fabs(eigenvalues[i] * this->spaceMag);

        if(evScale > dotP){
            if(this->extremum == 0){
                if(eigenvalues[i] < -(this->evMin)) isExtremum++;
            } else if(this->extremum == 1){
                if(eigenvalues[i] > this->evMin) isExtremum++;
            }
        }
    }
    double value = double(isExtremum)/ double(8);

    return value;
}

double UncertainRidges::computeRidge(Vector80d sampleVector){
    
    vec3 gradients[8];
    mat3 hessians[8];
    vec3 secGrads[8];
    double hasExtremum = 0.0;

    if(this->computeLines){
        computeGradients(sampleVector, gradients, hessians, secGrads, true);
        hasExtremum = computeParVectors(gradients, hessians, secGrads);
    } else {
        computeGradients(sampleVector, gradients, hessians, secGrads);

        if(this->useNewMethod){
            hasExtremum = computeRidgeSurfaceTest(gradients, hessians);
        } else {
            hasExtremum = computeRidgeSurface(gradients, hessians);
        }
    }
    
    return hasExtremum;
}

double UncertainRidges::computeRidge2D(Vector24d sampleVector){

    vec2 gradients[4];
    mat2 hessians[4];
    vec2 secGrads[4];
    double hasExtremum = 0.0;

    computeGradients2D(sampleVector, gradients, hessians, secGrads);

    if(this->useNewMethod){
        hasExtremum = computeRidgeLine2DTest(gradients, hessians, secGrads);
    } else {
        hasExtremum = computeRidgeLine2D(gradients, hessians, secGrads);
    }

    return hasExtremum;
}
