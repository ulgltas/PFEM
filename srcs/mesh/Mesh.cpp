#include "Mesh.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

#include <gmsh.h>


Mesh::Mesh(const MeshCreateInfo& meshInfos) :
m_hchar(meshInfos.hchar), m_alpha(meshInfos.alpha), m_omega(meshInfos.omega), m_gamma(meshInfos.gamma), m_boundingBox(meshInfos.boundingBox)
{
    loadFromFile(meshInfos.mshFile);
}

bool Mesh::addNodes(bool verboseOutput) noexcept
{
    assert(!m_elementsList.empty() && !m_nodesList.empty() && "There is no mesh!");
    assert(m_elementsDetJ.size() == m_elementsList.size() && "The determinant are not computed!");

    bool addedNodes = false;

    for(IndexType i = 0 ; i < m_elementsList.size() ; ++i)
    {
        //If an element is too big, we add a node at his center
        if(m_elementsDetJ[i]*getRefElementSize() > m_omega*std::pow(m_hchar, m_dim))
        {
            Node newNode = {};
            newNode.position.resize(m_dim);

            for(unsigned short k = 0 ; k < m_dim ; ++k)
            {
                newNode.position[k] = 0;
                for(unsigned short d = 0 ; d <= m_dim ; ++d)
                {
                    newNode.position[k] += m_nodesList[m_elementsList[i][d]].position[k];
                }
                newNode.position[k] /= (m_dim + 1);
            }

            newNode.states.resize(m_nodesList[0].states.size());
            for(unsigned short k = 0 ; k < m_nodesList[0].states.size() ; ++k)
            {
                newNode.states[k] = 0;
                for(unsigned short d = 0 ; d <= m_dim ; ++d)
                {
                    newNode.states[k] += m_nodesList[m_elementsList[i][d]].states[k];
                }
                newNode.states[k] /= (m_dim + 1);
            }

            m_nodesList.push_back(std::move(newNode));

            if(verboseOutput)
            {
                std::cout << "Adding node (" << "(";
                for(unsigned short d = 0 ; d < m_dim ; ++d)
                {
                    std::cout << m_nodesList[m_nodesList.size() - 1].position[d];
                    if(d == m_dim - 1)
                        std::cout << ")";
                    else
                        std::cout << ", ";
                }
                std::cout << std::endl;
            }

            addedNodes = true;
        }
    }

    return addedNodes;
}

bool Mesh::checkBoundingBox(bool verboseOutput) noexcept
{
    assert(!m_elementsList.empty() && !m_nodesList.empty() && "There is no mesh !");

    std::vector<bool> toBeDeleted(m_nodesList.size(), false);   //Should the node be deleted

    bool outofBBNodes = false;

    for(IndexType n = 0 ; n < m_nodesList.size() ; ++n)
    {
        //If the node is out of the bounding box, we delete it.
        // Bounding box fromat: [xmin, ymin, zmin, xmax, ymax, zmax]
        for(unsigned short d = 0 ; d < m_dim ; ++d)
        {
            if(m_nodesList[n].position[d] < m_boundingBox[d] ||
               m_nodesList[n].position[d] > m_boundingBox[d + m_dim])
            {
                toBeDeleted[n] = true;
                outofBBNodes = true;
                break;
            }
        }
    }

    m_nodesList.erase(
    std::remove_if(m_nodesList.begin(), m_nodesList.end(), [this, &toBeDeleted, verboseOutput](const Node& node)
    {
       if(toBeDeleted[&node - &*std::begin(m_nodesList)])
       {
           if(verboseOutput)
           {
               std::cout << "Removing out of bounding box node (";
                for(unsigned short d = 0 ; d < m_dim ; ++d)
                {
                    std::cout << node.position[d];
                    if(d == m_dim - 1)
                        std::cout << ")";
                    else
                        std::cout << ", ";
                }
                std::cout << std::endl;
           }

           return true;
       }
       else
           return false;
    }), m_nodesList.end());

    return outofBBNodes;
}

void Mesh::computeElementsDetJ() noexcept
{
    assert(!m_elementsList.empty() && !m_nodesList.empty() && "There is no mesh !");
    assert(!m_elementsJ.empty() && m_elementsJ.size() == m_elementsList.size());

    m_elementsDetJ.clear();
    m_elementsDetJ.resize(m_elementsList.size());

    #pragma omp parallel for default(shared)
    for(IndexType i = 0 ; i < m_elementsList.size() ; ++i)
    {
        if(m_dim == 2)
        {
            m_elementsDetJ[i] = m_elementsJ[i][0][0]*m_elementsJ[i][1][1]
                              - m_elementsJ[i][1][0]*m_elementsJ[i][0][1];
        }
        else
        {
            m_elementsDetJ[i] = m_elementsJ[i][0][0]*m_elementsJ[i][1][1]*m_elementsJ[i][2][2]
                              + m_elementsJ[i][0][1]*m_elementsJ[i][1][2]*m_elementsJ[i][2][0]
                              + m_elementsJ[i][0][2]*m_elementsJ[i][1][0]*m_elementsJ[i][2][1]
                              - m_elementsJ[i][2][0]*m_elementsJ[i][1][1]*m_elementsJ[i][0][2]
                              - m_elementsJ[i][2][1]*m_elementsJ[i][1][2]*m_elementsJ[i][0][0]
                              - m_elementsJ[i][2][2]*m_elementsJ[i][1][0]*m_elementsJ[i][0][1];
        }
    }
}

void Mesh::computeElementsInvJ() noexcept
{
    assert(!m_elementsList.empty() && !m_nodesList.empty() && "There is no mesh!");
    assert(!m_elementsJ.empty() && m_elementsJ.size() == m_elementsList.size());
    assert(m_elementsDetJ.size() == m_elementsList.size() && "The determinant are not computed!");

    m_elementsInvJ.clear();
    m_elementsInvJ.resize(m_elementsList.size());

    #pragma omp parallel for default(shared)
    for(IndexType i = 0 ; i < m_elementsList.size() ; ++i)
    {
        if(m_dim == 2)
        {
            m_elementsInvJ[i].resize(2);
            m_elementsInvJ[i][0].resize(2);
            m_elementsInvJ[i][1].resize(2);

            m_elementsInvJ[i][0][0] = m_elementsJ[i][1][1]/m_elementsDetJ[i];

            m_elementsInvJ[i][0][1] = - m_elementsJ[i][0][1]/m_elementsDetJ[i];

            m_elementsInvJ[i][1][0] = - m_elementsJ[i][1][0]/m_elementsDetJ[i];

            m_elementsInvJ[i][1][1] = m_elementsJ[i][0][0]/m_elementsDetJ[i];
        }
        else
        {
            m_elementsInvJ[i].resize(3);
            m_elementsInvJ[i][0].resize(3);
            m_elementsInvJ[i][1].resize(3);
            m_elementsInvJ[i][2].resize(3);

            m_elementsInvJ[i][0][0] = (m_elementsJ[i][1][1]*m_elementsJ[i][2][2]
                                    - m_elementsJ[i][1][2]*m_elementsJ[i][2][1])/m_elementsDetJ[i];

            m_elementsInvJ[i][0][1] = (m_elementsJ[i][2][1]*m_elementsJ[i][0][2]
                                    - m_elementsJ[i][2][2]*m_elementsJ[i][0][1])/m_elementsDetJ[i];

            m_elementsInvJ[i][0][2] = (m_elementsJ[i][0][1]*m_elementsJ[i][1][2]
                                    - m_elementsJ[i][0][2]*m_elementsJ[i][1][1])/m_elementsDetJ[i];

            m_elementsInvJ[i][1][0] = (m_elementsJ[i][2][0]*m_elementsJ[i][1][2]
                                    - m_elementsJ[i][1][0]*m_elementsJ[i][2][2])/m_elementsDetJ[i];

            m_elementsInvJ[i][1][1] = (m_elementsJ[i][0][0]*m_elementsJ[i][2][2]
                                    - m_elementsJ[i][2][0]*m_elementsJ[i][0][2])/m_elementsDetJ[i];

            m_elementsInvJ[i][1][2] = (m_elementsJ[i][1][0]*m_elementsJ[i][0][2]
                                    - m_elementsJ[i][0][0]*m_elementsJ[i][1][2])/m_elementsDetJ[i];

            m_elementsInvJ[i][2][0] = (m_elementsJ[i][1][0]*m_elementsJ[i][2][1]
                                    - m_elementsJ[i][2][0]*m_elementsJ[i][1][1])/m_elementsDetJ[i];

            m_elementsInvJ[i][2][1] = (m_elementsJ[i][2][0]*m_elementsJ[i][0][1]
                                    - m_elementsJ[i][0][0]*m_elementsJ[i][2][1])/m_elementsDetJ[i];

            m_elementsInvJ[i][2][2] = (m_elementsJ[i][0][0]*m_elementsJ[i][1][1]
                                    - m_elementsJ[i][1][0]*m_elementsJ[i][0][1])/m_elementsDetJ[i];
        }

    }
}

void Mesh::computeElementsJ() noexcept
{
    assert(!m_elementsList.empty() && !m_nodesList.empty() && "There is no mesh!");

    m_elementsJ.clear();
    m_elementsJ.resize(m_elementsList.size());

    #pragma omp parallel for default(shared)
    for(IndexType i = 0 ; i < m_elementsList.size() ; ++i)
    {
        if(m_dim == 2)
        {
            m_elementsJ[i].resize(2);
            m_elementsJ[i][0].resize(2);
            m_elementsJ[i][1].resize(2);

            double x0 = m_nodesList[m_elementsList[i][0]].position[0];
            double x1 = m_nodesList[m_elementsList[i][1]].position[0];
            double x2 = m_nodesList[m_elementsList[i][2]].position[0];
            double y0 = m_nodesList[m_elementsList[i][0]].position[1];
            double y1 = m_nodesList[m_elementsList[i][1]].position[1];
            double y2 = m_nodesList[m_elementsList[i][2]].position[1];

            m_elementsJ[i][0][0] = x1 - x0;
            m_elementsJ[i][0][1] = x2 - x0;
            m_elementsJ[i][1][0] = y1 - y0;
            m_elementsJ[i][1][1] = y2 - y0;
        }
        else
        {
            m_elementsJ[i].resize(3);
            m_elementsJ[i][0].resize(3);
            m_elementsJ[i][1].resize(3);
            m_elementsJ[i][2].resize(3);

            double x0 = m_nodesList[m_elementsList[i][0]].position[0];
            double x1 = m_nodesList[m_elementsList[i][1]].position[0];
            double x2 = m_nodesList[m_elementsList[i][2]].position[0];
            double x3 = m_nodesList[m_elementsList[i][3]].position[0];
            double y0 = m_nodesList[m_elementsList[i][0]].position[1];
            double y1 = m_nodesList[m_elementsList[i][1]].position[1];
            double y2 = m_nodesList[m_elementsList[i][2]].position[1];
            double y3 = m_nodesList[m_elementsList[i][3]].position[1];
            double z0 = m_nodesList[m_elementsList[i][0]].position[2];
            double z1 = m_nodesList[m_elementsList[i][1]].position[2];
            double z2 = m_nodesList[m_elementsList[i][2]].position[2];
            double z3 = m_nodesList[m_elementsList[i][3]].position[2];

            m_elementsJ[i][0][0] = x1 - x0;
            m_elementsJ[i][0][1] = x2 - x0;
            m_elementsJ[i][0][2] = x3 - x0;
            m_elementsJ[i][1][0] = y1 - y0;
            m_elementsJ[i][1][1] = y2 - y0;
            m_elementsJ[i][1][2] = y3 - y0;
            m_elementsJ[i][2][0] = z1 - z0;
            m_elementsJ[i][2][1] = z2 - z0;
            m_elementsJ[i][2][2] = z3 - z0;
        }
    }
}

//void Mesh::computeFreeSurfaceEdgeDetJ()
//{
//    assert(!m_freeSurfaceEdgesList.empty() && !m_nodesList.empty() && "There is no mesh !");
//
//    m_freeSurfaceEdgeDetJ.clear();
//    m_freeSurfaceEdgeDetJ.resize(m_freeSurfaceEdgesList.size());
//
//    #pragma omp parallel for default(shared)
//    for(IndexType i = 0 ; i < m_freeSurfaceEdgesList.size() ; ++i)
//    {
//        m_freeSurfaceEdgeDetJ[i] = 0; //TO DO
//    }
//}

void Mesh::computeMeshDim()
{
    int elementDim = -1;

    // loop over the dimension i to get the maximum element dimension in the mesh
    for(unsigned short i = 2 ; i <= 3 ; ++i)
    {
        std::vector<int> eleTypes;
        gmsh::model::mesh::getElementTypes(eleTypes, i);

        switch(eleTypes.size())
        {
            case 0:
                break;
            case 1:
                elementDim = i;
                break;
            default:
                throw std::runtime_error("do not use hybrid meshes for PFEM simulations!");
        }
    }

    if(elementDim == -1)
        throw std::runtime_error("there is no suitable elements in the .msh file!");
    else
        m_dim = static_cast<unsigned short>(elementDim);
}

void Mesh::loadFromFile(const std::string& fileName)
{
    m_nodesList.clear();

    gmsh::initialize();
    gmsh::option::setNumber("General.Terminal", 0);

    std::ifstream file(fileName);
    if(file.is_open())
        file.close();
    else
        throw std::runtime_error("the input .msh file does not exist!");

    gmsh::open(fileName);

    // Check that the mesh is not 3D
    computeMeshDim();

    if(m_boundingBox.size() != 2*m_dim)
        throw std::runtime_error("bad bounding box format! Format: [xmin, ymin, xmax, ymax]");

    // We retrieve the tags of the physical groups of dimension m_dim and
    // m_dim-1
    std::vector<std::pair<int, int>> physGroupHandlesHD;
    gmsh::model::getPhysicalGroups(physGroupHandlesHD, m_dim);

    std::vector<std::pair<int, int>> physGroupHandlesLD;
    gmsh::model::getPhysicalGroups(physGroupHandlesLD, m_dim - 1);

    //The file should contain physical group for the boundary, the free surface and the fluid

    for(auto physGroupLD : physGroupHandlesLD)
    {
        std::string name;
        gmsh::model::getPhysicalName(m_dim - 1, physGroupLD.second, name);
        if(name != "FreeSurface")
        {
            std::vector<double> coord;
            std::vector<std::size_t> dummyNodesTagsBoundary;
            gmsh::model::mesh::getNodesForPhysicalGroup(m_dim - 1, physGroupLD.second,
                                                        dummyNodesTagsBoundary, coord);

            for(std::size_t i = 0 ; i < dummyNodesTagsBoundary.size() ; ++i)
            {
                Node node = {};
                node.position.resize(m_dim);
                node.initialPosition.resize(m_dim);
                for(unsigned short d = 0 ; d < m_dim ; ++d)
                {
                    node.position[d] = coord[3*i + d];
                    node.initialPosition[d] = coord[3*i + d];
                }

                //If the nodes is already on the boundary, we do not add it twice
                if(std::find(m_nodesList.begin(), m_nodesList.end(), node) != m_nodesList.end())
                    continue;

                node.isBound = true;

                auto posBCinTagNames = std::find(m_tagNames.begin(), m_tagNames.end(), name);
                if(posBCinTagNames == std::end(m_tagNames))
                {
                    m_tagNames.push_back(name);
                    node.tag = m_tagNames.size() - 1;
                }
                else
                {
                    node.tag = static_cast<unsigned int>(std::distance(m_tagNames.begin(), posBCinTagNames));
                }

                m_nodesList.push_back(std::move(node));
            }
        }
    }

    for(auto physGroupHD : physGroupHandlesHD)
    {
        std::string name;
        gmsh::model::getPhysicalName(m_dim, physGroupHD.second, name);

        std::vector<std::size_t> dummyNodesTags;
        std::vector<double> coord;
        gmsh::model::mesh::getNodesForPhysicalGroup(m_dim, physGroupHD.second,
                                                    dummyNodesTags, coord);

        for(std::size_t i = 0 ; i < dummyNodesTags.size() ; ++i)
        {
            Node node = {};
            node.position.resize(m_dim);
            for(unsigned short d = 0 ; d < m_dim ; ++d)
                node.position[d] = coord[3*i + d];

            //If the nodes is already on the boundary, we do not add it twice
            if(std::find(m_nodesList.begin(), m_nodesList.end(), node) != m_nodesList.end())
                continue;

            node.isBound = false;

            auto posBCinTagNames = std::find(m_tagNames.begin(), m_tagNames.end(), name);
            if(posBCinTagNames == std::end(m_tagNames))
            {
                m_tagNames.push_back(name);
                node.tag = m_tagNames.size() - 1;
            }
            else
            {
                node.tag = static_cast<unsigned int>(std::distance(m_tagNames.begin(), posBCinTagNames));
            }

            m_nodesList.push_back(std::move(node));
        }
    }

#ifndef NDEBUG
    for(IndexType n = 0 ; n < m_nodesList.size() ; ++n)
    {
        for(IndexType n2 = 0 ; n2 < m_nodesList.size() ; ++n2)
        {
            if(n != n2)
            {
                if(m_nodesList[n] == m_nodesList[n2])
                {
                    std::cerr << "Duplicate node found: " << "(";
                    for(unsigned short d = 0 ; d < m_dim ; ++d)
                    {
                        std::cerr << m_nodesList[n].position[d];
                        if(d == m_dim - 1)
                            std::cout << ")";
                        else
                            std::cout << ", ";
                    }
                    std::cerr << std::endl;
                    throw std::runtime_error("duplicates nodes!");
                }
            }
        }
    }
    std::cout << "List of founded BC names: " << std::endl;
    for (auto& aString : m_tagNames)
    {
        std::cout << aString << std::endl;
    }
#endif

    if(m_nodesList.empty())
        throw std::runtime_error("no nodes loaded! Did you add the physical groups in the .geo file?");

    gmsh::finalize();

    triangulateAlphaShape();
    //computeFreeSurfaceEdgeDetJ();
    computeElementsJ();
    computeElementsDetJ();
    computeElementsInvJ();
}

void Mesh::remesh(bool verboseOutput)
{
    checkBoundingBox(verboseOutput);
    triangulateAlphaShape();

    if(removeNodes(verboseOutput))
        triangulateAlphaShape();

    computeElementsJ();
    computeElementsDetJ();

    if(addNodes(verboseOutput))
    {
        triangulateAlphaShape();
        computeElementsJ();
        computeElementsDetJ();
        computeElementsInvJ();
    }
    else
    {
        computeElementsInvJ();
    }
}

bool Mesh::removeNodes(bool verboseOutput) noexcept
{
    assert(!m_elementsList.empty() && !m_nodesList.empty() && "There is no mesh !");

    std::vector<bool> touched(m_nodesList.size(), false);       //Is the node next to a node which should be deleted
    std::vector<bool> toBeDeleted(m_nodesList.size(), false);   //Should the node be deleted

    bool removeNodes = false;

    for(IndexType i = 0 ; i < m_nodesList.size() ; ++i)
    {
        //We the node is free, it is not in an element; if a node is already tagged,
        //we can skip it (do not delete node twice)
        if(toBeDeleted[i] || m_nodesList[i].isFree)
            continue;

        for(unsigned int j = 0 ; j < m_nodesList[i].neighbourNodes.size() ; ++j)
        {
            double d = 0;

            for(unsigned short k = 0 ; k < m_dim ; ++k)
            {
                d +=(m_nodesList[i].position[k] - m_nodesList[m_nodesList[i].neighbourNodes[j]].position[k])
                   *(m_nodesList[i].position[k] - m_nodesList[m_nodesList[i].neighbourNodes[j]].position[k]);
            }
            d = std::sqrt(d);

            //Two nodes are too close.
            if(d <= m_gamma*m_hchar)
            {
                //If the neighbour nodes is touched, we delete the current nodes
                if(touched[m_nodesList[i].neighbourNodes[j]])
                {
                    //Do not delete bounded or free surface nodes
                    if(m_nodesList[i].isBound || m_nodesList[i].isOnFreeSurface)
                        continue;

                    toBeDeleted[i] = true;
                    removeNodes = true;
                }
                //If the neighbour nodes is not touched, we delete the neoghbour nodes
                else
                {
                    //Do not delete bounded or free surface nodes
                    if(m_nodesList[m_nodesList[i].neighbourNodes[j]].isBound ||
                       m_nodesList[m_nodesList[i].neighbourNodes[j]].isOnFreeSurface)
                        continue;

                    touched[i] = true;
                    toBeDeleted[m_nodesList[i].neighbourNodes[j]] = true;
                    removeNodes = true;
                }
            }
        }
    }

    m_nodesList.erase(
    std::remove_if(m_nodesList.begin(), m_nodesList.end(), [this, &toBeDeleted, verboseOutput](const Node& node)
    {
       if(toBeDeleted[&node - &*std::begin(m_nodesList)])
       {
           if(verboseOutput)
           {
                std::cout << "Removing node " << "(";
                for(unsigned short d = 0 ; d < m_dim ; ++d)
                {
                    std::cout << node.position[d];
                    if(d == m_dim - 1)
                        std::cout << ")";
                    else
                        std::cout << ", ";
                }
                std::cout << std::endl;
           }

           return true;
       }
       else
           return false;
    }), m_nodesList.end());

    return removeNodes;
}

void Mesh::restoreNodesList()
{
    if(m_nodesListSave.empty())
        throw std::runtime_error("the nodes list was not saved before or does not exist!");

    m_nodesList = m_nodesListSave;

    m_nodesListSave.clear();

    //computeFreeSurfaceEdgeDetJ();
    computeElementsJ();
    computeElementsDetJ();
    computeElementsInvJ();
}

void Mesh::saveNodesList()
{
    if(m_nodesList.empty())
        throw std::runtime_error("the nodes list does not exist!");

    m_nodesListSave = m_nodesList;
}

void Mesh::triangulateAlphaShape()
{
    if(m_dim == 2)
        triangulateAlphaShape2D();
    else
        triangulateAlphaShape3D();
}

void Mesh::updateNodesPosition(std::vector<double> deltaPos)
{
    if(deltaPos.size() != m_nodesList.size()*m_dim)
        throw std::runtime_error("invalid size of the deltaPos vector");

    #pragma omp parallel for default(shared)
    for(IndexType n = 0 ; n < m_nodesList.size() ; ++n)
    {
        if(!m_nodesList[n].isDirichlet)
        {
            for(unsigned short d = 0 ; d < m_dim ; ++d)
            {
                m_nodesList[n].position[d] += deltaPos[n + d*m_nodesList.size()];
            }
        }
    }

    //computeFreeSurfaceEdgeDetJ();
    computeElementsJ();
    computeElementsDetJ();
    computeElementsInvJ();
}

void Mesh::updateNodesPositionFromSave(std::vector<double> deltaPos)
{
    if(m_nodesListSave.empty())
        throw std::runtime_error("you did not save the nodes list!");
    else if(deltaPos.size() != m_nodesListSave.size()*m_dim)
        throw std::runtime_error("invalid size of the deltaPos vector");

    #pragma omp parallel for default(shared)
    for(IndexType n = 0 ; n < m_nodesList.size() ; ++n)
    {
        if(!m_nodesList[n].isDirichlet)
        {
            for(unsigned short d = 0 ; d < m_dim ; ++d)
            {
                m_nodesList[n].position[d] = m_nodesListSave[n].position[d] + deltaPos[n + d*m_nodesList.size()];
            }
        }
    }

    //computeFreeSurfaceEdgeDetJ();
    computeElementsJ();
    computeElementsDetJ();
    computeElementsInvJ();
}
