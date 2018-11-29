#include "Engine/engine.h"

using std::set_difference;
using std::inserter;

/**
 * @brief Engine Default constructor for class Engine
 */
Engine::Engine()
{

}

/**
 * @brief findInterestPoints Method for finding interest points for a mesh
 * @param theMesh Mesh sent by communicator for computing interest points
 * @param numRings Number of rings to be considered for the computation of neighbourhood
 * @param k Constant for Harris operator computation (Equation 3 in paper)
 * @return vector of integers with indixes of vertexes that are of interest
 */
vector<int> * Engine::findInterestPoints(Mesh * theMesh, int numRings, double k)
{
    Engine computations = Engine();
    MatrixXd vertexes = computations.getVertexesFromMesh(theMesh);
    MatrixXi faces = computations.getFacesFromMesh(theMesh);
    int numVertexes = vertexes.rows();
    VectorXd harrisValues(numVertexes); //Vector for storing values of harris operator for each vertex

    //For each vertex, compute harris operator
    for(int iVertex=0; iVertex< vertexes.rows(); iVertex++)
    {
        //Get indexes of faces that contain current vertex
        VectorXi facesForThisVertex = computations.getFacesForVertex(theMesh, iVertex);
        //Get indexes of direct neighbours:
        VectorXi neighbours = computations.getDirectNeighbours(iVertex, faces, facesForThisVertex);
        //Get indexes of vertexes in neighbourhood k
        VectorXi kRings = computations.getRings(iVertex, numRings, faces, neighbours, theMesh);
        //Get matrix with points in neighbourhood k (convert indexes to points)
        MatrixXd pointskRings = computations.getVertexesFromIndexes(kRings, theMesh);
        //Find location of current point in vector of indexes of neighbourhood k
        int currentVertexIndexInkRings = computations.getVertexIndexInNeighbourhood(iVertex, kRings);
		MatrixXd Centroid;
        //Center points
		MatrixXd centeredPoints = computations.centerNeighbourhood(pointskRings, Centroid);
        //Rotate
		MatrixXd rotatedPoints  = computations.rotateToFitPlane(centeredPoints,currentVertexIndexInkRings);
        //Fit surface to points
        MatrixXd fittedSurface = computations.fitQuadraticSurface(rotatedPoints,currentVertexIndexInkRings);
        //Find derivative of surface
        MatrixXd matrixE = computations.findderivativeEmatrix(fittedSurface);
        //Compute Harris operator
        double harrisOperator = computations.computeHarris(matrixE, k);
        //Store value of Harris operator for current point
        harrisValues(iVertex) = harrisOperator;
    }

    //Make pre - selection of interest points
    set <int> preSelected;
    bool discard(false);
    //Pre-selection of points
    for(int iVertex=0; iVertex< numVertexes; iVertex++)
    {
        discard = false;
        //Get indexes of faces that contain current vertex
        VectorXi facesForThisVertex = this->getFacesForVertex(theMesh, iVertex);
        //Get indexes of direct neighbours:
        VectorXi neighbours = this->getDirectNeighbours(iVertex, faces, facesForThisVertex);
        //For each point, evaluate if its Harris response is greater than the one of its direct neighbours
        for(int iNeighbour=0; iNeighbour < neighbours.size(); iNeighbour++)
        {
            if(harrisValues(iVertex) < harrisValues(neighbours(iNeighbour)))
            {
                discard = true;
                break;
            }
        }
        if(!discard)
        {
            preSelected.insert(iVertex);
        }
    }
    //Convert set to VectorXi
    int numPreselected = preSelected.size();
    VectorXi preSelectedVertexes(numPreselected);
    int ctrlVar1(0);
    for (set<int>::iterator it=preSelected.begin(); it!=preSelected.end(); ++it)
    {
        preSelectedVertexes(ctrlVar1) = *it;
        ctrlVar1++;
    }

    //Here, select interest points according to highest harris value and clustering (we have to receive another variable in this function)

    return NULL;
}

/**
 * @brief getVertexesFromMesh converts vector of vertexes of theMesh into an MatrixXd
 * @param theMesh is the mesh or surface being analyzed (read and sent from middleware)
 * @return Matrix with vertexes
 */
MatrixXd Engine::getVertexesFromMesh(Mesh * theMesh)
{
    int numPoints = theMesh->getAllVertexes()->size();
    double * currVertex = NULL;
    MatrixXd points(numPoints,3);
    for(int iVertex=0; iVertex<numPoints; iVertex++)
    {
        currVertex = theMesh->getVertex(iVertex)->getCoordinates();
        for(int j=0; j<3; j++)
        {
            points(iVertex,j) = currVertex[j];
        }
    }
    return points;
}

/**
 * @brief getFacesFromMesh converts vector of faces of theMesh into an MatrixXi
 * @param theMesh is the mesh or surface being analyzed (read and sent from middleware)
 * @return Matrix with faces
 */
MatrixXi Engine::getFacesFromMesh(Mesh * theMesh)
{
    int numFaces = theMesh->getAllFaces()->size();
    int * currFace = NULL;
    MatrixXi faces(numFaces, 3);
    for(int iFace=0; iFace<numFaces; iFace++)
    {
        currFace = theMesh->getFace(iFace)->getPointsInFace();
        for(int j=0; j<3;j++)
        {
            faces(iFace, j) = currFace[j];
        }
    }
    return faces;
}

/**
 * @brief getFacesForVertex gets the faces that contain certain vertex
 * @param theMesh is the mesh or surface being analyzed (read and sent from middleware)
 * @param pointIndex Index of the point for which faces have to be found
 * @return Vector with indexes of faces in which vertex is involved
 */
VectorXi Engine::getFacesForVertex(Mesh * theMesh, int pointIndex)
{
    vector<int> facesForVertex = theMesh->getVertex(pointIndex)->getFaces();
    VectorXi facesForThisVertex(facesForVertex.size());
    for(int iF=0; iF<facesForVertex.size(); iF++)
    {
        facesForThisVertex(iF) = facesForVertex.at(iF);
    }
    return facesForThisVertex;
}

/**
 * @brief getDirectNeighbours finds the direct neighbours of a vertex
 * @param vertex is the index of the vertex
 * @param faces is the matrix of faces
 * @param facesThatContainPoint is the vector that contains indexes of faces that contain vertex
 * @return Indexes of direct neighbours as a vector
 */
VectorXi Engine::getDirectNeighbours(int iVertex, MatrixXi faces, VectorXi facesThatContainPoint)
{
    int numFacesWithVertex = facesThatContainPoint.size();
    set<int> directNeighbours;
    for(int i=0; i<numFacesWithVertex; i++)
    {
        for(int j=0; j< 3; j++)
        {
            if(faces(facesThatContainPoint(i), j) != iVertex)
            {
                directNeighbours.insert(faces(facesThatContainPoint(i), j));
            }
        }
    }
    int numDirectNeighbours = directNeighbours.size();
    VectorXi vertexesFirstNeighbourhood(numDirectNeighbours);
    int ctrlVar1(0);
    for (set<int>::iterator it=directNeighbours.begin(); it!=directNeighbours.end(); ++it)
    {
        vertexesFirstNeighbourhood(ctrlVar1) = *it;
        ctrlVar1++;
    }
    return vertexesFirstNeighbourhood;
}

/**
 * @brief getRings gets neighbourhood k for point with index vertex
 * @param vertex is the index of the vertex
 * @param k is the depth of the ring neighbourhood
 * @param faces is the matrix of faces
 * @param firstNeighbours is a vector containing the indexes of the direct neighbours
 * @param theMesh is the mesh or surface being analyzed (read and sent from middleware)
 * @return a vector with indexes of neighbours until depth k (includes all points within ring k and also ring k)
 */
VectorXi Engine::getRings(int vertex, int k, MatrixXi faces, VectorXi firstNeighbours, Mesh * theMesh)
{
    set <int> s0;
    s0.insert(vertex);
    set <int> s1;
    set <int> sAccum;
    for(int iNei=0;iNei<firstNeighbours.size(); iNei++)
    {
        s1.insert(firstNeighbours(iNei));
    }
    set <int> result = s0;
    result.insert(s1.begin(), s1.end());
    set <int> tempSet1;
    set <int> tempSet2;
    for(int ring=2; ring<k; ring++)
    {
        for (set<int>::iterator it=s1.begin(); it!=s1.end(); ++it)
        {
            VectorXi facesThatContainPoint = this->getFacesForVertex(theMesh, *it);
            VectorXi someNeighbours = this->getDirectNeighbours(*it, faces, facesThatContainPoint);
            for(int i=0; i<someNeighbours.size(); i++)
            {
                sAccum.insert(someNeighbours(i));
                result.insert(someNeighbours(i));
            }
        }
        set_difference(sAccum.begin(), sAccum.end(), s1.begin(), s1.end(), inserter(tempSet1, tempSet1.end()));
        set_difference(tempSet1.begin(), tempSet1.end(), s0.begin(), s0.end(), inserter(tempSet2, tempSet2.end()));
        s0 = s1;
        s1 = tempSet2;
        tempSet1.clear();
        tempSet2.clear();
    }
    int kNeighboursSize = result.size();
    VectorXi kNeighbours(kNeighboursSize);
    int ctrlVar1(0);
    for (set<int>::iterator it=result.begin(); it!=result.end(); ++it)
    {
        kNeighbours(ctrlVar1) = *it;
        ctrlVar1++;
    }
    return kNeighbours;
}

/**
 * @brief getVertexesFromIndexes gets matrix with vertexes corresponding to specific indexes
 * @param indexes indexes of vertexes
 * @param theMesh is the mesh or surface being analyzed (read and sent from middleware)
 * @return Matrix with points corresponding to given indexes
 */
MatrixXd Engine::getVertexesFromIndexes(VectorXi indexes, Mesh * theMesh)
{
    MatrixXd points(indexes.size(), 3);
    for(int iP=0; iP<indexes.size(); iP++)
    {
        double * xyz =  theMesh->getVertex(indexes(iP))->getCoordinates();
        points(iP, 0) = xyz[0];
        points(iP, 1) = xyz[1];
        points(iP, 2) = xyz[2];
    }
    return points;
}

/**
 * @brief getVertexFromMeshAsVector3d Gets a vertex as a vector from its position or index
 * @param position is the index of the vertex that is required
 * @param theMesh is the mesh or surface being analyzed (read and sent from middleware)
 * @return
 */
Vector3d Engine::getVertexFromMeshAsVector3d(int position, Mesh * theMesh)
{
    double * xyz = theMesh->getVertex(position)->getCoordinates();
    Vector3d vertexVector;
    vertexVector(0) = xyz[0];
    vertexVector(1) = xyz[1];
    vertexVector(2) = xyz[2];
    return vertexVector;
}

/**
 * @brief getVertexIndexInNeighbourhood Gets index of vertex in the neighbourhood vector
 * @param vertexIndex is the index of vertex for which neighbourhood was computed
 * @param indexesOfNeighbours is the vector with the indexes of the neighbours
 * @return index of vertexIndex in indexOfNeighbours
 */
int Engine::getVertexIndexInNeighbourhood(int vertexIndex, VectorXi indexesOfNeighbours)
{
    int indexOfVertex(0);
    for(int i=0; i<indexesOfNeighbours.size(); i++)
    {
        if(indexesOfNeighbours(i)==vertexIndex)
        {
            indexOfVertex = i;
            break;
        }
    }
    return indexOfVertex;
}


/**
 * @brief centerNeighbourhood :
 *        Centers a Neighborhood around its centroid and translate
 *        the set of points to the origin
 * @param Neighbourhood
 * @return
 */
MatrixXd Engine::centerNeighbourhood(MatrixXd Neighbourhood , MatrixXd& Centroid )
{
    // Calculate the mean of all the values in the colums of the neibourghood
    Centroid = Neighbourhood.colwise().sum() / Neighbourhood.rows();

    // Center the Neighbour values with the calculated mean
    MatrixXd centeredPoints = MatrixXd::Ones(Neighbourhood.rows(),1)
                                                        * Centroid ;
    centeredPoints = Neighbourhood - centeredPoints;
    return centeredPoints;
}


/**
 * @brief findBestFittingPlane:
 *        Apply Principal Component Analysis to the set of points and
 *        choose the eigenvector with the lowest associated eigenvalue
 *        as the normal of the fitting plane.
 * @param CenteredNeighbourhood
 * @return
 */
MatrixXd Engine::rotateToFitPlane(MatrixXd centeredPoints, int analizedPointIndex)
{
    // As we centered the data before applying the PCA algorithm the covariance
    // matrix can be calculated applying Cxy=P.t()*P;
    MatrixXd covarianceMatrix = centeredPoints.transpose()*centeredPoints;
    SelfAdjointEigenSolver<MatrixXd> covarianceDescomposition(covarianceMatrix);

    // The function SelfAdjointEigenSolver returns the eigenvalues and eigenvectors
    // sorted in increasing order, so we can take the first eigenvector as the normal
    // of the fitting plane.
    MatrixXd normalPlane = covarianceDescomposition.eigenvectors().col(0).matrix();

    // To perform the rotation we take the eigenvectors in decreasing order
    MatrixXd eigenVectors = covarianceDescomposition.eigenvectors().rowwise().reverse();

    // Check if the rotation matrix fulfil the right hand law
    // We take the analysis point and we check if the normal plane points upwards.
    MatrixXd analizedPoint = centeredPoints.row(analizedPointIndex); // - Centroid;
    MatrixXd zEigenVector  = covarianceDescomposition.eigenvectors().row(2).matrix();
    double normalDirection = (analizedPoint * zEigenVector.transpose() )(0,0);

    // If the direction of the normal plane is contrary to the analized point
    // We must invert z direction and exchange the x and y columns
    MatrixXd rotationMatrix = eigenVectors;
    if( normalDirection < 0 )
    {
        rotationMatrix = -eigenVectors;
        rotationMatrix.col(0) = eigenVectors.col(1);
        rotationMatrix.col(1) = eigenVectors.col(0);
    }

    // Rotate the centered points with te resulting rotationMatrix
    MatrixXd rotatedPoints = centeredPoints * rotationMatrix;
    return rotatedPoints;
}


/**
 * @brief fitQuadraticSurface
 *        Apply Least Squares to fit a quadratic surface to the rotated
 *        points of the NEibourhood.
 * @param rotatedPoints     : the rotated points of the Neibourhood
 * @param analizedPointIndex  : the vertex of analisys
 * @return
 */
MatrixXd Engine::fitQuadraticSurface(MatrixXd rotatedPoints, int analizedPointIndex)
{
    MatrixXd analizedPoint = rotatedPoints.row(analizedPointIndex);
    analizedPoint(0,2) = 0;
    // Center the points in the XY plane
    MatrixXd centeredXY = MatrixXd::Ones(rotatedPoints.rows(),1) * analizedPoint;
    centeredXY = rotatedPoints - centeredXY;

    // Solve the equation AX = b knowing that A are the coeffitiens of:
    // [x*x x*y y*y x y 1] X = z
    MatrixXd A = MatrixXd::Ones(rotatedPoints.rows(),6);
    A.col(0) = rotatedPoints.col(0).cwiseProduct(rotatedPoints.col(0));
    A.col(1) = rotatedPoints.col(0).cwiseProduct(rotatedPoints.col(1));
    A.col(2) = rotatedPoints.col(1).cwiseProduct(rotatedPoints.col(1));
    A.col(3) = rotatedPoints.col(0);
    A.col(4) = rotatedPoints.col(1);
    MatrixXd b = rotatedPoints.col(2);
    MatrixXd X = A.fullPivLu().solve(b);

    // X = [p1/2 p2 p3/2 p4 p5 p6] , so  we multiply X(0) and X(2) by 2
    X(0) = X(0) * 2;
    X(2) = X(2) * 2;
    return X;
}

/**
 * @brief findderivativeEmatrix formulates the derivative matrix for
 *        the harris operator
 * @param X : the parameters of the Quadratic Surface.
 * @return
 */
MatrixXd Engine::findderivativeEmatrix(MatrixXd X)
{
    // Recover the parameter to perfor equation 10-12 of the paper
    double p1, p2, p3, p4, p5;
    p1 = X(0,0);
    p2 = X(1,0);
    p3 = X(2,0);
    p4 = X(3,0);
    p5 = X(4,0);

    double A = p4*p4 + 2*p1*p1 + 2*p2*p2;
    double B = p5*p5 + 2*p2*p2 + 2*p3*p3;
    double C = p4*p5 + 2*p1*p2 + 2*p2*p3;

    // Creating matrix for the harris operator
    MatrixXd E(2,2);
    E(0,0) = A;
    E(0,1) = C;
    E(1,0) = C;
    E(1,1) = B;

    return E;
}

/**
 * @brief computeHarris computes the Harris operator for the current point
 * @param E Matrix with parameters A,B,C obtained from surface fitting
 * @param k Paramter for Harris operator calculation according to formula (3)
 * @return Value of Harris operator according to equation (3)
 */
double Engine::computeHarris(MatrixXd E, double k)
{
    double A = E(0,0);
    double C = E(0,1);
    double B = E(1,1);
    double harrisOperator = (A*B - C*C) - k*(A+B)*(A+B); //det(E) - k*(tr(E))^2
    return harrisOperator;
}


