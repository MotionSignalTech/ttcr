//
//  Grid2Duc.h
//  ttcr
//
//  Created by Bernard Giroux on 2014-02-24.
//  Copyright (c) 2014 Bernard Giroux. All rights reserved.
//

/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef ttcr_Grid2Duc_h
#define ttcr_Grid2Duc_h

#include <array>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#ifdef VTK
#include "vtkCellData.h"
#include "vtkDoubleArray.h"
#include "vtkIntArray.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkProbeFilter.h"
#include "vtkRectilinearGrid.h"
#include "vtkUnstructuredGrid.h"
#include "vtkSmartPointer.h"
#include "vtkTriangle.h"
#include "vtkXMLRectilinearGridWriter.h"
#include "vtkXMLUnstructuredGridWriter.h"
#endif

#include <boost/math/special_functions/sign.hpp>

#include "Grid2D.h"
#include "Grad.h"

namespace ttcr {
    
    template<typename T1, typename T2, typename NODE, typename S>
    class Grid2Duc : public Grid2D<T1,T2,S> {
    public:
        Grid2Duc(const std::vector<S>& no,
                 const std::vector<triangleElem<T2>>& tri,
                 const size_t nt=1) :
        nThreads(nt),
        nPrimary(static_cast<T2>(no.size())),
        nodes(std::vector<NODE>(no.size(), NODE(nt))),
        slowness(std::vector<T1>(tri.size())),
        neighbors(std::vector<std::vector<T2>>(tri.size())),
        triangles(), virtualNodes()
        {
            for (auto it=tri.begin(); it!=tri.end(); ++it) {
                triangles.push_back( *it );
            }
        }
        
        virtual ~Grid2Duc() {}
        
        void setSlowness(const T1 s) {
            for ( size_t n=0; n<slowness.size(); ++n ) {
                slowness[n] = s;
            }
        }
        
        void setSlowness(const T1 *s, const size_t ns) {
            if ( slowness.size() != ns ) {
                throw std::length_error("Error: slowness vectors of incompatible size.");
            }
            for ( size_t n=0; n<slowness.size(); ++n ) {
                slowness[n] = s[n];
            }
        }
        
        void setSlowness(const std::vector<T1>& s) {
            if ( slowness.size() != s.size() ) {
                throw std::length_error("Error: slowness vectors of incompatible size.");
            }
            for ( size_t n=0; n<slowness.size(); ++n ) {
                slowness[n] = s[n];
            }
        }
        
        void setTT(const T1 tt, const size_t nn, const size_t nt=0) {
            nodes[nn].setTT(tt, nt);
        }
        
        size_t getNumberOfNodes() const { return nodes.size(); }
        size_t getNumberOfCells() const { return triangles.size(); }
        
        const T1 getXmin() const {
            T1 xmin = nodes[0].getX();
            for ( auto it=nodes.begin(); it!=nodes.end(); ++it )
                xmin = xmin<it->getX() ? xmin : it->getX();
            return xmin;
        }
        const T1 getXmax() const {
            T1 xmax = nodes[0].getX();
            for ( auto it=nodes.begin(); it!=nodes.end(); ++it )
                xmax = xmax>it->getX() ? xmax : it->getX();
            return xmax;
        }
        const T1 getZmin() const {
            T1 zmin = nodes[0].getZ();
            for ( auto it=nodes.begin(); it!=nodes.end(); ++it )
                zmin = zmin<it->getZ() ? zmin : it->getZ();
            return zmin;
        }
        const T1 getZmax() const {
            T1 zmax = nodes[0].getZ();
            for ( auto it=nodes.begin(); it!=nodes.end(); ++it )
                zmax = zmax>it->getZ() ? zmax : it->getZ();
            return zmax;
        }
        
        void saveTT(const std::string &, const int, const size_t nt=0,
                    const bool vtkFormat=0) const;
        
#ifdef VTK
        void saveModelVTU(const std::string &, const bool saveSlowness=true,
                          const bool savePhysicalEntity=false) const;
        void saveModelVTR(const std::string &, const double*,
                          const bool saveSlowness=true) const;
#endif
        
        const size_t getNthreads() const { return nThreads; }
        
    protected:
        const size_t nThreads;
        T2 nPrimary;
        mutable std::vector<NODE> nodes;
        std::vector<T1> slowness;
        std::vector<std::vector<T2>> neighbors;  // nodes common to a cell
        std::vector<triangleElemAngle<T1,T2>> triangles;
        std::map<T2, virtualNode<T1,NODE>> virtualNodes;
        
        void buildGridNeighbors() {
            // Index the neighbors nodes of each cell
            for ( T2 n=0; n<nodes.size(); ++n ) {
                for ( size_t n2=0; n2<nodes[n].getOwners().size(); ++n2) {
                    neighbors[ nodes[n].getOwners()[n2] ].push_back(n);
                }
            }
        }
        
        T1 computeDt(const NODE& source, const S& node,
                     const size_t cellNo) const {
            return slowness[cellNo] * source.getDistance( node );
        }
        
        T1 computeDt(const NODE& source, const NODE& node,
                     const size_t cellNo) const {
            return slowness[cellNo] * source.getDistance( node );
        }
        
        T2 getCellNo(const S& pt) const {
            for ( T2 n=0; n<triangles.size(); ++n ) {
                if ( insideTriangle(pt, n) ) {
                    return n;
                }
            }
            return -1;
        }
        
        T1 getTraveltime(const S& Rx,
                         const std::vector<NODE>& nodes,
                         const size_t threadNo) const;
        
        T1 getTraveltime(const S& Rx,
                         const std::vector<NODE>& nodes,
                         T2& nodeParentRx,
                         T2& cellParentRx,
                         const size_t threadNo) const;
        
        
        void checkPts(const std::vector<sxz<T1>>&) const;
        void checkPts(const std::vector<sxyz<T1>>&) const;
        
        bool insideTriangle(const sxz<T1>&, const T2) const;
        bool insideTriangle(const sxyz<T1>&, const T2) const;
        
        void processObtuse();
        
        void localSolver(NODE *vertexC, const size_t threadNo) const;
        
        void getRaypath(const std::vector<sxz<T1>>& Tx,
                        const sxz<T1> &Rx,
                        std::vector<sxz<T1>> &r_data,
                        const size_t threadNo) const;
        
        void getRaypath_ho(const std::vector<sxz<T1>>& Tx,
                           const sxz<T1> &Rx,
                           std::vector<sxz<T1>> &r_data,
                           const size_t threadNo) const;
        
        bool findIntersection(const T2 i0, const T2 i1,
                              const sxz<T1> &g,
                              sxz<T1> &curr_pt) const;
        
        T2 findNextCell1(const T2 i0, const T2 i1, const T2 nodeNo) const;
        T2 findNextCell2(const T2 i0, const T2 i1, const T2 cellNo) const;
        
        void getNeighborNodes(const T2 cellNo, std::set<NODE*> &nnodes) const;

    };
    
    template<typename T1, typename T2, typename NODE, typename S>
    T1 Grid2Duc<T1,T2,NODE,S>::getTraveltime(const S& Rx,
                                             const std::vector<NODE>& nodes,
                                             const size_t threadNo) const {
        
        for ( size_t nn=0; nn<nodes.size(); ++nn ) {
            if ( nodes[nn] == Rx ) {
                return nodes[nn].getTT(threadNo);
            }
        }
        
        size_t cellNo = getCellNo( Rx );
        size_t neibNo = neighbors[cellNo][0];
        T1 dt = computeDt(nodes[neibNo], Rx, cellNo);
        
        T1 traveltime = nodes[neibNo].getTT(threadNo)+dt;
        for ( size_t k=1; k< neighbors[cellNo].size(); ++k ) {
            neibNo = neighbors[cellNo][k];
            dt = computeDt(nodes[neibNo], Rx, cellNo);
            if ( traveltime > nodes[neibNo].getTT(threadNo)+dt ) {
                traveltime =  nodes[neibNo].getTT(threadNo)+dt;
            }
        }
        return traveltime;
    }
    
    
    template<typename T1, typename T2, typename NODE, typename S>
    T1 Grid2Duc<T1,T2,NODE,S>::getTraveltime(const S& Rx,
                                             const std::vector<NODE>& nodes,
                                             T2& nodeParentRx, T2& cellParentRx,
                                             const size_t threadNo) const {
        
        for ( size_t nn=0; nn<nodes.size(); ++nn ) {
            if ( nodes[nn] == Rx ) {
                nodeParentRx = nodes[nn].getNodeParent(threadNo);
                cellParentRx = nodes[nn].getCellParent(threadNo);
                return nodes[nn].getTT(threadNo);
            }
        }
        
        T2 cellNo = getCellNo( Rx );
        T2 neibNo = neighbors[cellNo][0];
        T1 dt = computeDt(nodes[neibNo], Rx, cellNo);
        
        T1 traveltime = nodes[neibNo].getTT(threadNo)+dt;
        nodeParentRx = neibNo;
        cellParentRx = cellNo;
        for ( size_t k=1; k< neighbors[cellNo].size(); ++k ) {
            neibNo = neighbors[cellNo][k];
            dt = computeDt(nodes[neibNo], Rx, cellNo);
            if ( traveltime > nodes[neibNo].getTT(threadNo)+dt ) {
                traveltime =  nodes[neibNo].getTT(threadNo)+dt;
                nodeParentRx = neibNo;
            }
        }
        return traveltime;
    }
    
    
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::checkPts(const std::vector<sxz<T1>>& pts) const {
        
        for (size_t n=0; n<pts.size(); ++n) {
            bool found = false;
            // check first if point is on a node
            for ( T2 nt=0; nt<nodes.size(); ++nt ) {
                if ( nodes[nt] == pts[n]) {
                    found = true;
                    break;
                }
            }
            if ( found == false ) {
                for ( T2 nt=0; nt<triangles.size(); ++nt ) {
                    if ( insideTriangle(pts[n], nt) ) {
                        found = true;
                    }
                }
            }
            if ( found == false ) {
                std::ostringstream msg;
                msg << "Error: Point no " << n << " (" << pts[n].x << ", "<< pts[n] .z << ") outside mesh.";
                throw std::runtime_error(msg.str());
            }
        }
    }
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::checkPts(const std::vector<sxyz<T1>>& pts) const {
        
        for (size_t n=0; n<pts.size(); ++n) {
            bool found = false;
            // check first if point is on a node
            for ( T2 nt=0; nt<nodes.size(); ++nt ) {
                if ( nodes[nt] == pts[n]) {
                    found = true;
                    break;
                }
            }
            if ( found == false ) {
                for ( T2 nt=0; nt<triangles.size(); ++nt ) {
                    if ( insideTriangle(pts[n], nt) ) {
                        found = true;
                    }
                }
            }
            if ( found == false ) {
                std::ostringstream msg;
                msg << "Error: Point no " << n << " (" << pts[n].x << ", "<< pts[n] .y << ", "<< pts[n] .z << ") outside mesh.";
                throw std::runtime_error(msg.str());
            }
        }
    }
    
    
    template<typename T1, typename T2, typename NODE, typename S>
    bool Grid2Duc<T1,T2,NODE,S>::insideTriangle(const sxz<T1>& v, const T2 nt) const {
        
        
        // from http://mathworld.wolfram.com/TriangleInterior.html
        
        sxz<T1> v0 = { nodes[ triangles[nt].i[0] ].getX(),
            nodes[ triangles[nt].i[0] ].getZ() };
        sxz<T1> v1 = { nodes[ triangles[nt].i[1] ].getX()-v0.x,
            nodes[ triangles[nt].i[1] ].getZ()-v0.z };
        sxz<T1> v2 = { nodes[ triangles[nt].i[2] ].getX()-v0.x,
            nodes[ triangles[nt].i[2] ].getZ()-v0.z };
        
        T1 invDenom = 1. / det(v1, v2);
        T1 a = (det(v, v2) - det(v0, v2)) * invDenom;
        T1 b = -(det(v, v1) - det(v0, v1)) * invDenom;
        return (a >= 0.) && (b >= 0.) && (a + b < 1.);
    }
    
    template<typename T1, typename T2, typename NODE, typename S>
    bool Grid2Duc<T1,T2,NODE,S>::insideTriangle(const sxyz<T1>& p, const T2 nt) const {
        
        
        
        sxyz<T1> a = { nodes[ triangles[nt].i[0] ].getX(),
            nodes[ triangles[nt].i[0] ].getY(),
            nodes[ triangles[nt].i[0] ].getZ() };
        sxyz<T1> b = { nodes[ triangles[nt].i[1] ].getX(),
            nodes[ triangles[nt].i[1] ].getY(),
            nodes[ triangles[nt].i[1] ].getZ() };
        sxyz<T1> c = { nodes[ triangles[nt].i[2] ].getX(),
            nodes[ triangles[nt].i[2] ].getY(),
            nodes[ triangles[nt].i[2] ].getZ() };
        
        // Translate point and triangle so that point lies at origin
        a -= p; b -= p; c -= p;
        // Compute normal vectors for triangles pab and pbc
        sxyz<T1> u = cross(b, c);
        sxyz<T1> v = cross(c, a);
        // Make sure they are both pointing in the same direction
        if (dot(u, v) < static_cast<T1>(0.0)) return false;
        // Compute normal vector for triangle pca
        sxyz<T1> w = cross(a, b);
        // Make sure it points in the same direction as the first two
        if (dot(u, w) < static_cast<T1>(0.0)) return false;
        // Otherwise P must be in (or on) the triangle
        return true;
    }
    
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::saveTT(const std::string &fname, const int all,
                                        const size_t nt, const bool vtkFormat) const {
        
        if (vtkFormat) {
#ifdef VTK
            std::string filename = fname+".vtu";
            
            vtkSmartPointer<vtkUnstructuredGrid> ugrid =
            vtkSmartPointer<vtkUnstructuredGrid>::New();
            
            vtkSmartPointer<vtkPoints> newPts =
            vtkSmartPointer<vtkPoints>::New();
            vtkSmartPointer<vtkDoubleArray> newScalars =
            vtkSmartPointer<vtkDoubleArray>::New();
            
            newScalars->SetName("Travel time");
            
            double xyz[3];
            T2 nMax = nPrimary;  // only primary are saved
            for (size_t n=0; n<nMax; ++n) {
                xyz[0] = nodes[n].getX();
                xyz[1] = nodes[n].getY();
                xyz[2] = nodes[n].getZ();
                newPts->InsertPoint(n, xyz);
                newScalars->InsertValue(n, nodes[n].getTT(nt) );
            }
            
            ugrid->SetPoints(newPts);
            ugrid->GetPointData()->SetScalars(newScalars);
            
            vtkSmartPointer<vtkTriangle> tri =
            vtkSmartPointer<vtkTriangle>::New();
            for (size_t n=0; n<triangles.size(); ++n) {
                tri->GetPointIds()->SetId(0, triangles[n].i[0] );
                tri->GetPointIds()->SetId(1, triangles[n].i[1] );
                tri->GetPointIds()->SetId(2, triangles[n].i[2] );
                
                ugrid->InsertNextCell( tri->GetCellType(), tri->GetPointIds() );
            }
            vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer =
            vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
            
            writer->SetFileName( filename.c_str() );
            //		writer->SetInputConnection( ugrid->GetProducerPort() );
            writer->SetInputData( ugrid );
            writer->SetDataModeToBinary();
            writer->Update();
#else
            std::cerr << "VTK not included during compilation.\nNothing saved.\n";
#endif
        } else {
            std::string filename = fname+".dat";
            std::ofstream fout(filename.c_str());
            fout.precision(12);
            T2 nMax = nPrimary;
            if ( all == 1 ) {
                nMax = static_cast<T2>(nodes.size());
            }
            for ( T2 n=0; n<nMax; ++n ) {
                fout << nodes[n].getX() << '\t'
                << nodes[n].getZ() << '\t'
                << nodes[n].getTT(nt) << '\n';
            }
            fout.close();
        }
    }
    
#ifdef VTK
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::saveModelVTU(const std::string &fname,
                                              const bool saveSlowness,
                                              const bool savePhysicalEntity) const {
        
        vtkSmartPointer<vtkUnstructuredGrid> ugrid =
        vtkSmartPointer<vtkUnstructuredGrid>::New();
        
        vtkSmartPointer<vtkPoints> newPts =
        vtkSmartPointer<vtkPoints>::New();
        
        double xyz[3];
        T2 nMax = nPrimary;  // only primary are saved
        for (size_t n=0; n<nMax; ++n) {
            xyz[0] = nodes[n].getX();
            xyz[1] = nodes[n].getY();
            xyz[2] = nodes[n].getZ();
            newPts->InsertPoint(n, xyz);
        }
        
        ugrid->SetPoints(newPts);
        
        vtkSmartPointer<vtkTriangle> tri =
        vtkSmartPointer<vtkTriangle>::New();
        vtkSmartPointer<vtkDoubleArray> data = vtkSmartPointer<vtkDoubleArray>::New();
        if ( saveSlowness ) {
            data->SetName("Slowness");
            
            for (size_t n=0; n<triangles.size(); ++n) {
                tri->GetPointIds()->SetId(0, triangles[n].i[0] );
                tri->GetPointIds()->SetId(1, triangles[n].i[1] );
                tri->GetPointIds()->SetId(2, triangles[n].i[2] );
                
                ugrid->InsertNextCell( tri->GetCellType(), tri->GetPointIds() );
                data->InsertNextValue( slowness[n] );
            }
        } else {
            data->SetName("Velocity");
            
            for (size_t n=0; n<triangles.size(); ++n) {
                tri->GetPointIds()->SetId(0, triangles[n].i[0] );
                tri->GetPointIds()->SetId(1, triangles[n].i[1] );
                tri->GetPointIds()->SetId(2, triangles[n].i[2] );
                
                ugrid->InsertNextCell( tri->GetCellType(), tri->GetPointIds() );
                data->InsertNextValue( 1./slowness[n] );
            }
        }
        
        ugrid->GetCellData()->SetScalars(data);
        
        vtkSmartPointer<vtkIntArray> data_pe = vtkSmartPointer<vtkIntArray>::New();
        if ( savePhysicalEntity ) {
            data_pe->SetName("Physical entity");
            for (size_t n=0; n<triangles.size(); ++n) {
                data_pe->InsertNextValue(triangles[n].physical_entity );
            }
            ugrid->GetCellData()->AddArray(data_pe);
        }
        
        vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer =
        vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
        
        writer->SetFileName( fname.c_str() );
        writer->SetInputData( ugrid );
        writer->SetDataModeToBinary();
        writer->Update();
    }
    
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::saveModelVTR(const std::string &fname,
                                              const double *d,
                                              const bool saveSlowness) const {
        
        double x[] = { nodes[0].getX(), nodes[0].getX(),
            0.0, 0.0,
            nodes[0].getZ(), nodes[0].getZ() };
        
        for (size_t n=1; n<nPrimary; ++n) {
            x[0] = x[0] < nodes[n].getX() ? x[0] : nodes[n].getX();
            x[1] = x[1] > nodes[n].getX() ? x[1] : nodes[n].getX();
            
            x[4] = x[4] < nodes[n].getZ() ? x[4] : nodes[n].getZ();
            x[5] = x[5] > nodes[n].getZ() ? x[5] : nodes[n].getZ();
        }
        
        int nn[3];
        nn[0] = 1 + (x[1]-x[0])/d[0];
        nn[1] = 1;
        nn[2] = 1 + (x[5]-x[4])/d[2];
        
        vtkSmartPointer<vtkDoubleArray> xCoords = vtkSmartPointer<vtkDoubleArray>::New();
        for (size_t n=0; n<nn[0]; ++n)
            xCoords->InsertNextValue( x[0] + n*d[0] );
        vtkSmartPointer<vtkDoubleArray> yCoords = vtkSmartPointer<vtkDoubleArray>::New();
        for (size_t n=0; n<nn[1]; ++n)
            yCoords->InsertNextValue( x[2] + n*d[1] );
        vtkSmartPointer<vtkDoubleArray> zCoords = vtkSmartPointer<vtkDoubleArray>::New();
        for (size_t n=0; n<nn[2]; ++n)
            zCoords->InsertNextValue( x[4] + n*d[2] );
        
        vtkSmartPointer<vtkRectilinearGrid> rgrid = vtkSmartPointer<vtkRectilinearGrid>::New();
        rgrid->SetDimensions( nn );
        rgrid->SetXCoordinates(xCoords);
        rgrid->SetYCoordinates(yCoords);
        rgrid->SetZCoordinates(zCoords);
        
        vtkSmartPointer<vtkDoubleArray> data = vtkSmartPointer<vtkDoubleArray>::New();
        
        sxz<T1> pt;
        if ( saveSlowness ) {
            data->SetName("Slowness");
            for ( size_t n=0; n<rgrid->GetNumberOfCells(); ++n ) {
                
                rgrid->GetCell(n)->GetBounds(x);
                pt.x = 0.5*(x[0]+x[1]);
                pt.z = 0.5*(x[4]+x[5]);
                
                for ( T2 nt=0; nt<triangles.size(); ++nt ) {
                    if ( insideTriangle(pt, nt) ) {
                        data->InsertNextValue( slowness[nt] );
                        break;
                    }
                }
            }
        } else {
            data->SetName("Velocity");
            for ( size_t n=0; n<rgrid->GetNumberOfCells(); ++n ) {
                
                rgrid->GetCell(n)->GetBounds(x);
                pt.x = 0.5*(x[0]+x[1]);
                pt.z = 0.5*(x[4]+x[5]);
                
                for ( T2 nt=0; nt<triangles.size(); ++nt ) {
                    if ( insideTriangle(pt, nt) ) {
                        data->InsertNextValue( 1./slowness[nt] );
                        break;
                    }
                }
            }
        }
        
        rgrid->GetCellData()->SetScalars( data );
        
        vtkSmartPointer<vtkXMLRectilinearGridWriter> writer =
        vtkSmartPointer<vtkXMLRectilinearGridWriter>::New();
        
        writer->SetFileName( fname.c_str() );
        writer->SetInputData( rgrid );
        writer->SetDataModeToBinary();
        writer->Update();
        
    }
#endif
    
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::processObtuse() {
        
        const double pi2 = pi / 2.;
        
        for ( T2 ntri=0; ntri<triangles.size(); ++ntri ) {
            
            for ( T2 n=0; n<3; ++n ) {
                if ( triangles[ntri].a[n] > pi2 ) {
                    
                    // look for opposite triangle
                    
                    T2 i0 = triangles[ntri].i[n];
                    T2 i1 = triangles[ntri].i[(n+1)%3];
                    T2 i2 = triangles[ntri].i[(n+2)%3];
                    
                    T2 oppositeTriangle;
                    bool found = false;
                    for ( size_t n1=0; n1<nodes[i1].getOwners().size(); ++n1) {
                        for ( size_t n2=0; n2<nodes[i2].getOwners().size(); ++n2) {
                            if ( nodes[i2].getOwners()[n2] == nodes[i1].getOwners()[n1]) {
                                oppositeTriangle = nodes[i2].getOwners()[n2];
                                found = true;
                                break;
                            }
                            
                        }
                        if ( found ) break;
                    }
                    
                    if ( !found ) continue; // no opposite triangle, must be on edge of domain.  No correction applied.
                    
                    
                    // find opposite node
                    T2 i3 = triangles[oppositeTriangle].i[0];
                    if ( i3 == i1 || i3 == i2 )
                        i3 = triangles[oppositeTriangle].i[1];
                    else if ( i3 == i1 || i3 == i2 )
                        i3 = triangles[oppositeTriangle].i[2];
                    
                    virtualNode<T1,NODE> vn;
                    
                    // keep i1 & try replacing i2 with i3
                    vn.node1 = &(nodes[i1]);
                    vn.node2 = &(nodes[i3]);
                    
                    // distance between node 1 & 3 (opposite of node 0)
                    T1 a = nodes[i1].getDistance( nodes[i3] );
                    
                    // distance between node 0 & 3 (opposite of node 1)
                    T1 b = nodes[i0].getDistance( nodes[i3] );
                    
                    // distance between node 0 & 1 (opposite of node 3)
                    T1 c = nodes[i0].getDistance( nodes[i1] );
                    
                    // angle at node 0
                    T1 a0 = acos((b*b + c*c - a*a)/(2.*b*c));
                    
                    //				std::cout << nodes[i0].getX() << '\t' << nodes[i0].getZ() << "\t-\t"
                    //				<< nodes[i1].getX() << '\t' << nodes[i1].getZ() << "\t-\t"
                    //				<< nodes[i2].getX() << '\t' << nodes[i2].getZ() << "\t-\t"
                    //				<< nodes[i3].getX() << '\t' << nodes[i3].getZ();
                    
                    
                    if ( a0 > pi2 ) { // still obtuse -> replace i1 instead of i2 with i3
                        
                        vn.node1 = &(nodes[i3]);
                        vn.node2 = &(nodes[i2]);
                        
                        // distance between node 2 & 3 (opposite of node 0)
                        a = nodes[i2].getDistance( nodes[i3]);
                        
                        // distance between node 0 & 2 (opposite of node 1)
                        b = nodes[i0].getDistance( nodes[i2]);
                        
                        // distance between node 0 & 3 (opposite of node 2)
                        c = nodes[i0].getDistance( nodes[i3]);
                        
                        a0 = acos((b*b + c*c - a*a)/(2.*b*c));
                        
                        
                        //					std::cout << "\t\tswapped";
                    }
                    //				std::cout << "\n\n";
                    
                    vn.a[0] = a0;
                    vn.a[1] = acos((c*c + a*a - b*b)/(2.*a*c));
                    vn.a[2] = acos((a*a + b*b - c*c)/(2.*a*b));
                    
                    vn.e[0] = a;
                    vn.e[1] = b;
                    vn.e[2] = c;
                    
                    virtualNodes.insert( std::pair<T2, virtualNode<T1,NODE>>(ntri, vn) );
                    
                }
            }
        }
    }
    
    
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::localSolver(NODE *vertexC,
                                             const size_t threadNo) const {
        
        static const double pi2 = pi / 2.;
        T2 i0, i1, i2;
        NODE *vertexA, *vertexB;
        T1 a, b, c, alpha, beta;
        
        for ( size_t no=0; no<vertexC->getOwners().size(); ++no ) {
            
            T2 triangleNo = vertexC->getOwners()[no];
            
            for ( i0=0; i0<3; ++i0 ) {
                if ( vertexC->getGridIndex() == triangles[triangleNo].i[i0] ) break;
            }
            
            if ( triangles[triangleNo].a[i0] > pi/2 && !virtualNodes.empty() ) {
                
                virtualNode<T1,NODE> vn = virtualNodes.at(triangleNo);
                
                vertexA = vn.node1;
                vertexB = vn.node2;
                
                c = vn.e[0];
                a = vn.e[1];
                b = vn.e[2];
                
                alpha = vn.a[2];
                beta = vn.a[1];
            } else {
                
                i1 = (i0+1)%3;
                i2 = (i0+2)%3;
                
                vertexA = &(nodes[triangles[triangleNo].i[i1]]);
                vertexB = &(nodes[triangles[triangleNo].i[i2]]);
                
                c = triangles[triangleNo].l[i0];
                a = triangles[triangleNo].l[i1];
                b = triangles[triangleNo].l[i2];
                
                alpha = triangles[triangleNo].a[i2];
                beta = triangles[triangleNo].a[i1];
            }
            
            if ( fabs(vertexB->getTT(threadNo)-vertexA->getTT(threadNo)) <= c*slowness[triangleNo]) {
                
                T1 theta = asin( fabs(vertexB->getTT(threadNo)-vertexA->getTT(threadNo))/
                                (c*slowness[triangleNo]) );
                
                if ( ((0.>alpha-pi2?0.:alpha-pi2)<=theta && theta<=(pi2-beta) ) ||
                    ((alpha-pi2)<=theta && theta<=(0.<pi2-beta?0.:pi2-beta)) ) {
                    T1 h = a*sin(alpha-theta);
                    T1 H = b*sin(beta+theta);
                    
                    T1 t = 0.5*(h*slowness[triangleNo]+vertexB->getTT(threadNo)) +
                    0.5*(H*slowness[triangleNo]+vertexA->getTT(threadNo));
                    
                    if ( t<vertexC->getTT(threadNo) )
                        vertexC->setTT(t, threadNo);
                } else {
                    T1 t = vertexA->getTT(threadNo) + b*slowness[triangleNo];
                    t = t<vertexB->getTT(threadNo) + a*slowness[triangleNo] ? t :
                    vertexB->getTT(threadNo) + a*slowness[triangleNo];
                    if ( t<vertexC->getTT(threadNo) )
                        vertexC->setTT(t, threadNo);
                }
            } else {
                T1 t = vertexA->getTT(threadNo) + b*slowness[triangleNo];
                t = t<vertexB->getTT(threadNo) + a*slowness[triangleNo] ? t :
                vertexB->getTT(threadNo) + a*slowness[triangleNo];
                if ( t<vertexC->getTT(threadNo) )
                    vertexC->setTT(t, threadNo);
            }
        }
    }
    
    
    
    
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::getRaypath(const std::vector<sxz<T1>>& Tx,
                                            const sxz<T1> &Rx,
                                            std::vector<sxz<T1>> &r_data,
                                            const size_t threadNo) const {
        
        T1 minDist = small;
        r_data.push_back( Rx );
        
        for ( size_t ns=0; ns<Tx.size(); ++ns ) {
            if ( Rx == Tx[ns] ) {
                return;
            }
        }
        
        std::vector<bool> txOnNode( Tx.size(), false );
        std::vector<T2> txNode( Tx.size() );
        std::vector<T2> txCell( Tx.size() );
        for ( size_t nt=0; nt<Tx.size(); ++nt ) {
            for ( T2 nn=0; nn<nodes.size(); ++nn ) {
                if ( nodes[nn] == Tx[nt] ) {
                    txOnNode[nt] = true;
                    txNode[nt] = nn;
                    break;
                }
            }
        }
        for ( size_t nt=0; nt<Tx.size(); ++nt ) {
            if ( !txOnNode[nt] ) {
                txCell[nt] = getCellNo( Tx[nt] );
            }
        }
        
        T2 cellNo, nodeNo;
        sxz<T1> curr_pt( Rx );
        
        bool onNode=false;
        for ( T2 nn=0; nn<nodes.size(); ++nn ) {
            if ( nodes[nn] == curr_pt ) {
                nodeNo = nn;
                onNode = true;
                break;
            }
        }
        if ( !onNode ) {
            cellNo = getCellNo( curr_pt );
        }
        
        Grad2D<T1> grad2d;
        
        bool reachedTx = false;
        bool onEdge = false;
        std::array<T2,2> edgeNodes;
        
        while ( reachedTx == false ) {
            
            if ( onNode ) {
                
                // find cell for which gradient intersect opposing segment
                bool foundIntersection = false;
                std::vector<sxz<T1>> grads;
                for ( auto nc=nodes[nodeNo].getOwners().begin(); nc!=nodes[nodeNo].getOwners().end(); ++nc ) {
                    
                    T2 nb[2];
                    size_t n=0;
                    for (auto nn=neighbors[*nc].begin(); nn!=neighbors[*nc].end(); ++nn ) {
                        if ( *nn != nodeNo ) {
                            nb[n++] = *nn;
                        }
                    }
                    if ( nb[0]>nb[1] ) std::swap(nb[0], nb[1]);
                    
                    sxz<T1> g = grad2d.ls_grad(nodes[ nodeNo ], nodes[ nb[0] ], nodes[ nb[1] ], threadNo);
                    
                    sxz<T1> v1 = { nodes[ nb[0] ].getX() - nodes[ nodeNo ].getX(),
                        nodes[ nb[0] ].getZ() - nodes[ nodeNo ].getZ() };
                    sxz<T1> v2 = { nodes[ nb[1] ].getX() - nodes[ nodeNo ].getX(),
                        nodes[ nb[1] ].getZ() - nodes[ nodeNo ].getZ() };
                    
                    g.normalize();
                    v1.normalize();
                    v2.normalize();
                    
                    T1 theta1 = acos( dot(v1, g) );
                    T1 theta2 = acos( dot(v1, v2) );
                    
                    //				std::cout << '\n';
                    //				std::cout << "Node: " << nodes[ nodeNo ].getX() << ' ' << nodes[ nodeNo ].getZ() << '\t' << nodes[ nodeNo ].getTT(threadNo) << '\n';
                    //				std::cout << "n0  : " << nodes[ nb[0] ].getX() << ' ' << nodes[ nb[0] ].getZ() << '\t' << nodes[ nb[0] ].getTT(threadNo) << '\n';
                    //				std::cout << "n1  : " << nodes[ nb[1] ].getX() << ' ' << nodes[ nb[1] ].getZ() << '\t' << nodes[ nb[1] ].getTT(threadNo) << '\n';
                    //
                    //				std::cout << "grad: " << grad << '\n';
                    //				std::cout << "v1  : " << v1 << '\n';
                    //				std::cout << "v2  : " << v2 << '\n';
                    //
                    //				std::cout << "theta1: " << theta1 << '\n';
                    //				std::cout << "theta2: " << theta2 << '\n';
                    
                    if ( theta1 > theta2 ) {
                        grads.push_back( g );
                        continue;
                    }
                    
                    if ( boost::math::sign( cross(v1, g) ) != boost::math::sign( cross(v1, v2) ) ) {
                        grads.push_back( g );
                        continue;
                    }
                    
                    foundIntersection = true;
                    
                    bool break_flag = findIntersection(nb[0], nb[1], g, curr_pt);
                    
                    r_data.push_back( curr_pt );
                    if ( break_flag ) break;
                    
                    onEdge = true;
                    edgeNodes[0] = nb[0];
                    edgeNodes[1] = nb[1];
                    
                    // find next cell
                    cellNo = findNextCell1(nb[0], nb[1], nodeNo);
                    if ( cellNo == std::numeric_limits<T2>::max() ) {
                        std::cout << "\n\nWarning: finding raypath failed to converge for Rx "
                        << Rx.x << ' ' << Rx.z << std::endl;
                        r_data.resize(1);
                        r_data[0] = Rx;
                        reachedTx = true;
                    }
                    break;
                }
                
                if ( foundIntersection == false ) {
                    
                    // compute average gradient
                    sxz<T1> g = { 0., 0. };
                    for ( size_t n=0; n<grads.size(); ++n ) {
                        g.x += grads[n].x;
                        g.z += grads[n].z;
                    }
                    g.x /= grads.size();
                    g.z /= grads.size();
                    
                    
                    for ( auto nc=nodes[nodeNo].getOwners().begin(); nc!=nodes[nodeNo].getOwners().end(); ++nc ) {
                        
                        T2 nb[2];
                        size_t n=0;
                        for (auto nn=neighbors[*nc].begin(); nn!=neighbors[*nc].end(); ++nn ) {
                            if ( *nn != nodeNo ) {
                                nb[n++] = *nn;
                            }
                        }
                        if ( nb[0]>nb[1] ) std::swap(nb[0], nb[1]);
                        
                        sxz<T1> v1 = { nodes[ nb[0] ].getX() - nodes[ nodeNo ].getX(),
                            nodes[ nb[0] ].getZ() - nodes[ nodeNo ].getZ() };
                        sxz<T1> v2 = { nodes[ nb[1] ].getX() - nodes[ nodeNo ].getX(),
                            nodes[ nb[1] ].getZ() - nodes[ nodeNo ].getZ() };
                        
                        g.normalize();
                        v1.normalize();
                        v2.normalize();
                        
                        T1 theta1 = acos( dot(v1, g) );
                        T1 theta2 = acos( dot(v1, v2) );
                        
                        if ( theta1 > theta2 ) {
                            continue;
                        }
                        
                        if ( boost::math::sign( cross(v1, g) ) != boost::math::sign( cross(v1, v2) ) ) {
                            continue;
                        }
                        
                        foundIntersection = true;
                        
                        bool break_flag = findIntersection(nb[0], nb[1], g, curr_pt);
                        
                        r_data.push_back( curr_pt );
                        if ( break_flag ) break;
                        
                        onEdge = true;
                        edgeNodes[0] = nb[0];
                        edgeNodes[1] = nb[1];
                        
                        // find next cell
                        cellNo = findNextCell1(nb[0], nb[1], nodeNo);
                        if ( cellNo == std::numeric_limits<T2>::max() ) {
                            std::cout << "\n\nWarning: finding raypath failed to converge for Rx "
                            << Rx.x << ' ' << Rx.z << std::endl;
                            r_data.resize(1);
                            r_data[0] = Rx;
                            reachedTx = true;
                        }
                        break;
                    }
                }
                if ( foundIntersection == false ) {
                    std::cout << "\n\nWarning: finding raypath failed to converge for Rx "
                    << Rx.x << ' ' << Rx.z << std::endl;
                    r_data.resize(1);
                    r_data[0] = Rx;
                    reachedTx = true;
                }
                
            } else {
                
                sxz<T1> g = grad2d.ls_grad(nodes[neighbors[cellNo][0]],
                                           nodes[neighbors[cellNo][1]],
                                           nodes[neighbors[cellNo][2]], threadNo);
                g.normalize();
                //			std::cout << g.x << ' ' << g.z << '\n';
                
                // we have 3 segments that we might intersect
                T2 ind[3][2] = { {neighbors[cellNo][0], neighbors[cellNo][1]},
                    {neighbors[cellNo][0], neighbors[cellNo][2]},
                    {neighbors[cellNo][1], neighbors[cellNo][2]} };
                
                for ( size_t ns=0; ns<3; ++ns )
                    if ( ind[ns][0]>ind[ns][1] )
                        std::swap( ind[ns][0], ind[ns][1] );
                
                //			std::cout << "\n\nplot(" << curr_pt.x << "," << curr_pt.z << ", 'o')\nhold on\naxis equal\n";
                //			std::cout << "plot([" << curr_pt.x << " " << curr_pt.x+10*g.x << "],["
                //			<< curr_pt.z << " " << curr_pt.z+10*g.z << "], 'r-')\n";
                
                
                sxz<T1> pt_i;
                T1 m1, b1, m2, b2;
                bool foundIntersection = false;
                for ( size_t ns=0; ns<3; ++ns ) {
                    
                    //				std::cout << "\nplot([" << nodes[ ind[ns][0] ].getX() << ' ' << nodes[ ind[ns][1] ].getX() << "], ["
                    //				<< nodes[ ind[ns][0] ].getZ() << ' ' << nodes[ ind[ns][1] ].getZ() << "], '-')\n";
                    
                    // equation of the edge segment
                    T1 den = nodes[ ind[ns][1] ].getX() - nodes[ ind[ns][0] ].getX();
                    
                    if ( den == 0.0 ) {
                        m1 = INFINITY;
                        b1 = nodes[ ind[ns][1] ].getX();
                    } else {
                        m1 = ( nodes[ ind[ns][1] ].getZ() - nodes[ ind[ns][0] ].getZ() ) / den;
                        b1 = nodes[ ind[ns][1] ].getZ() - m1*nodes[ ind[ns][1] ].getX();
                    }
                    
                    // equation of the vector starting at curr_pt & pointing along gradient
                    if ( g.x == 0.0 ) {
                        m2 = INFINITY;
                        b2 = curr_pt.x;
                    } else {
                        m2 = g.z/g.x;
                        b2 = curr_pt.z - m2*curr_pt.x;
                    }
                    
                    if ( onEdge && ind[ns][0]==edgeNodes[0] && ind[ns][1]==edgeNodes[1] ) {
                        
                        //					std::cout << "m: " << m1 << ' ' << m2 << '\n';
                        if ( fabs(m1-m2)<small ) {
                            // curr_pt is on an edge and gradient is along the edge
                            // den is the direction of vector P0->P1 along x
                            if ( boost::math::sign(den) == boost::math::sign(g.x) ) {
                                curr_pt.x = nodes[ ind[ns][1] ].getX();
                                curr_pt.z = nodes[ ind[ns][1] ].getZ();
                                r_data.push_back( curr_pt );
                                foundIntersection = true;
                                break;
                            } else {
                                curr_pt.x = nodes[ ind[ns][0] ].getX();
                                curr_pt.z = nodes[ ind[ns][0] ].getZ();
                                r_data.push_back( curr_pt );
                                foundIntersection = true;
                                break;
                            }
                            
                        }
                        continue;
                    }
                    // intersection of edge segment & gradient vector
                    if ( m1 == INFINITY ) {
                        pt_i.x = b1;
                        pt_i.z = m2*pt_i.x + b2;
                    } else if ( m2 == INFINITY ) {
                        pt_i.x = b2;
                        pt_i.z = m1*pt_i.x + b1;
                    } else {
                        pt_i.x = (b2-b1)/(m1-m2);
                        pt_i.z = m2*pt_i.x + b2;
                    }
                    
                    sxz<T1> vec(pt_i.x-curr_pt.x, pt_i.z-curr_pt.z);
                    if ( dot(vec, g) <= 0.0 ) {
                        // we are not pointing in the same direction
                        continue;
                    }
                    
                    
                    //				std::cout << "plot(" << pt_i.x << "," << pt_i.z << ", 'go')\n";
                    
                    
                    if (((pt_i.x<=nodes[ ind[ns][1] ].getX() && pt_i.x>=nodes[ ind[ns][0] ].getX()) ||
                         (pt_i.x>=nodes[ ind[ns][1] ].getX() && pt_i.x<=nodes[ ind[ns][0] ].getX())) &&
                        ((pt_i.z<=nodes[ ind[ns][0] ].getZ() && pt_i.z>=nodes[ ind[ns][1] ].getZ()) ||
                         (pt_i.z>=nodes[ ind[ns][0] ].getZ() && pt_i.z<=nodes[ ind[ns][1] ].getZ())))
                    {
                        foundIntersection = true;
                        r_data.push_back( pt_i );
                        curr_pt = pt_i;
                        
                        onEdge = true;
                        edgeNodes[0] = ind[ns][0];
                        edgeNodes[1] = ind[ns][1];
                        
                        // find next cell
                        cellNo = findNextCell2(ind[ns][0], ind[ns][1], cellNo);
                        if ( cellNo == std::numeric_limits<T2>::max() ) {
                            std::cout << "\n\nWarning: finding raypath failed to converge for Rx "
                            << Rx.x << ' ' << Rx.z << std::endl;
                            r_data.resize(1);
                            r_data[0] = Rx;
                            reachedTx = true;
                        }
                        break;
                    }
                    
                }
                if ( foundIntersection == false ) {
                    
                    // we must be on an edge with gradient pointing slightly outside triangle
                    sxz<T1> vec(nodes[ edgeNodes[1] ].getX() - nodes[ edgeNodes[0] ].getX(),
                                nodes[ edgeNodes[1] ].getZ() - nodes[ edgeNodes[0] ].getZ());
                    
                    if ( dot(vec, g) > 0.0 ) {
                        curr_pt.x = nodes[ edgeNodes[1] ].getX();
                        curr_pt.z = nodes[ edgeNodes[1] ].getZ();
                        r_data.push_back( curr_pt );
                        foundIntersection = true;
                    } else {
                        curr_pt.x = nodes[ edgeNodes[0] ].getX();
                        curr_pt.z = nodes[ edgeNodes[0] ].getZ();
                        r_data.push_back( curr_pt );
                        foundIntersection = true;
                    }
                }
                
            }
            
            onNode=false;
            for ( T2 nn=0; nn<nodes.size(); ++nn ) {
                if ( nodes[nn] == curr_pt ) {
                    //                std::cout << nodes[nn].getX() << ' ' << nodes[nn].getZ() << '\n';
                    nodeNo = nn;
                    onNode = true;
                    onEdge = false;
                    break;
                }
            }
            
            if ( onNode ) {
                for ( size_t nt=0; nt<Tx.size(); ++nt ) {
                    if ( curr_pt.getDistance( Tx[nt] ) < minDist ) {
                        reachedTx = true;
                        break;
                    }
                }
            } else {
                for ( size_t nt=0; nt<Tx.size(); ++nt ) {
                    if ( txOnNode[nt] ) {
                        for ( auto nc=nodes[txNode[nt]].getOwners().begin();
                             nc!=nodes[txNode[nt]].getOwners().end(); ++nc ) {
                            if ( cellNo == *nc ) {
                                r_data.push_back( Tx[nt] );
                                reachedTx = true;
                                break;
                            }
                        }
                    } else {
                        if ( cellNo == txCell[nt] ) {
                            r_data.push_back( Tx[nt] );
                            reachedTx = true;
                        }
                    }
                    if ( reachedTx ) break;
                }
            }
        }
    }
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::getRaypath_ho(const std::vector<sxz<T1>>& Tx,
                                               const sxz<T1> &Rx,
                                               std::vector<sxz<T1>> &r_data,
                                               const size_t threadNo) const {
        
        T1 minDist = small;
        r_data.push_back( Rx );
        
        for ( size_t ns=0; ns<Tx.size(); ++ns ) {
            if ( Rx == Tx[ns] ) {
                return;
            }
        }
        
        std::vector<bool> txOnNode( Tx.size(), false );
        std::vector<T2> txNode( Tx.size() );
        std::vector<T2> txCell( Tx.size() );
        for ( size_t nt=0; nt<Tx.size(); ++nt ) {
            for ( T2 nn=0; nn<nodes.size(); ++nn ) {
                if ( nodes[nn] == Tx[nt] ) {
                    txOnNode[nt] = true;
                    txNode[nt] = nn;
                    break;
                }
            }
        }
        for ( size_t nt=0; nt<Tx.size(); ++nt ) {
            if ( !txOnNode[nt] ) {
                txCell[nt] = getCellNo( Tx[nt] );
            }
        }
        
        T2 cellNo, nodeNo;
        sxz<T1> curr_pt( Rx );
        
        bool onNode=false;
        for ( T2 nn=0; nn<nodes.size(); ++nn ) {
            if ( nodes[nn] == curr_pt ) {
                nodeNo = nn;
                onNode = true;
                break;
            }
        }
        if ( !onNode ) {
            cellNo = getCellNo( curr_pt );
        }
        
        Grad2D_ho<T1,NODE> grad2d;
        
        bool reachedTx = false;
        bool onEdge = false;
        std::array<T2,2> edgeNodes;
        
        while ( reachedTx == false ) {
            
            if ( onNode ) {
                
                // find cell for which gradient intersect opposing segment
                bool foundIntersection = false;
                std::vector<sxz<T1>> grads;
                for ( auto nc=nodes[nodeNo].getOwners().begin(); nc!=nodes[nodeNo].getOwners().end(); ++nc ) {
                    
                    T2 nb[2];
                    size_t n=0;
                    for (auto nn=neighbors[*nc].begin(); nn!=neighbors[*nc].end(); ++nn ) {
                        if ( *nn != nodeNo ) {
                            nb[n++] = *nn;
                        }
                    }
                    if ( nb[0]>nb[1] ) std::swap(nb[0], nb[1]);
                    
                    std::set<NODE*> nnodes;
                    getNeighborNodes(*nc, nnodes);
                    
                    sxz<T1> g = grad2d.ls_grad(nnodes, threadNo);
                    
                    sxz<T1> v1 = { nodes[ nb[0] ].getX() - nodes[ nodeNo ].getX(),
                        nodes[ nb[0] ].getZ() - nodes[ nodeNo ].getZ() };
                    sxz<T1> v2 = { nodes[ nb[1] ].getX() - nodes[ nodeNo ].getX(),
                        nodes[ nb[1] ].getZ() - nodes[ nodeNo ].getZ() };
                    
                    g.normalize();
                    v1.normalize();
                    v2.normalize();
                    
                    T1 theta1 = acos( dot(v1, g) );
                    T1 theta2 = acos( dot(v1, v2) );
                    
                    //				std::cout << '\n';
                    //				std::cout << "Node: " << nodes[ nodeNo ].getX() << ' ' << nodes[ nodeNo ].getZ() << '\t' << nodes[ nodeNo ].getTT(threadNo) << '\n';
                    //				std::cout << "n0  : " << nodes[ nb[0] ].getX() << ' ' << nodes[ nb[0] ].getZ() << '\t' << nodes[ nb[0] ].getTT(threadNo) << '\n';
                    //				std::cout << "n1  : " << nodes[ nb[1] ].getX() << ' ' << nodes[ nb[1] ].getZ() << '\t' << nodes[ nb[1] ].getTT(threadNo) << '\n';
                    //
                    //				std::cout << "grad: " << grad << '\n';
                    //				std::cout << "v1  : " << v1 << '\n';
                    //				std::cout << "v2  : " << v2 << '\n';
                    //
                    //				std::cout << "theta1: " << theta1 << '\n';
                    //				std::cout << "theta2: " << theta2 << '\n';
                    
                    if ( theta1 > theta2 ) {
                        grads.push_back( g );
                        continue;
                    }
                    
                    if ( boost::math::sign( cross(v1, g) ) != boost::math::sign( cross(v1, v2) ) ) {
                        grads.push_back( g );
                        continue;
                    }
                    
                    foundIntersection = true;
                    
                    bool break_flag = findIntersection(nb[0], nb[1], g, curr_pt);
                    
                    r_data.push_back( curr_pt );
                    if ( break_flag ) break;
                    
                    onEdge = true;
                    edgeNodes[0] = nb[0];
                    edgeNodes[1] = nb[1];
                    
                    // find next cell
                    cellNo = findNextCell1(nb[0], nb[1], nodeNo);
                    if ( cellNo == std::numeric_limits<T2>::max() ) {
                        std::cout << "\n\nWarning: finding raypath failed to converge for Rx "
                        << Rx.x << ' ' << Rx.z << std::endl;
                        r_data.resize(1);
                        r_data[0] = Rx;
                        reachedTx = true;
                    }
                    break;
                }
                
                if ( foundIntersection == false ) {
                    
                    // compute average gradient
                    sxz<T1> g = { 0., 0. };
                    for ( size_t n=0; n<grads.size(); ++n ) {
                        g.x += grads[n].x;
                        g.z += grads[n].z;
                    }
                    g.x /= grads.size();
                    g.z /= grads.size();
                    
                    
                    for ( auto nc=nodes[nodeNo].getOwners().begin(); nc!=nodes[nodeNo].getOwners().end(); ++nc ) {
                        
                        T2 nb[2];
                        size_t n=0;
                        for (auto nn=neighbors[*nc].begin(); nn!=neighbors[*nc].end(); ++nn ) {
                            if ( *nn != nodeNo ) {
                                nb[n++] = *nn;
                            }
                        }
                        if ( nb[0]>nb[1] ) std::swap(nb[0], nb[1]);
                        
                        sxz<T1> v1 = { nodes[ nb[0] ].getX() - nodes[ nodeNo ].getX(),
                            nodes[ nb[0] ].getZ() - nodes[ nodeNo ].getZ() };
                        sxz<T1> v2 = { nodes[ nb[1] ].getX() - nodes[ nodeNo ].getX(),
                            nodes[ nb[1] ].getZ() - nodes[ nodeNo ].getZ() };
                        
                        g.normalize();
                        v1.normalize();
                        v2.normalize();
                        
                        T1 theta1 = acos( dot(v1, g) );
                        T1 theta2 = acos( dot(v1, v2) );
                        
                        if ( theta1 > theta2 ) {
                            continue;
                        }
                        
                        if ( boost::math::sign( cross(v1, g) ) != boost::math::sign( cross(v1, v2) ) ) {
                            continue;
                        }
                        
                        foundIntersection = true;
                        
                        bool break_flag = findIntersection(nb[0], nb[1], g, curr_pt);
                        
                        r_data.push_back( curr_pt );
                        if ( break_flag ) break;
                        
                        onEdge = true;
                        edgeNodes[0] = nb[0];
                        edgeNodes[1] = nb[1];
                        
                        // find next cell
                        cellNo = findNextCell1(nb[0], nb[1], nodeNo);
                        if ( cellNo == std::numeric_limits<T2>::max() ) {
                            std::cout << "\n\nWarning: finding raypath failed to converge for Rx "
                            << Rx.x << ' ' << Rx.z << std::endl;
                            r_data.resize(1);
                            r_data[0] = Rx;
                            reachedTx = true;
                        }
                        break;
                    }
                }
                if ( foundIntersection == false ) {
                    std::cout << "\n\nWarning: finding raypath failed to converge for Rx "
                    << Rx.x << ' ' << Rx.z << std::endl;
                    r_data.resize(1);
                    r_data[0] = Rx;
                    reachedTx = true;
                }
                
            } else {
                
                std::set<NODE*> nnodes;
                getNeighborNodes(cellNo, nnodes);
                
                sxz<T1> g = grad2d.ls_grad(nnodes, threadNo);
                
                g.normalize();
                //			std::cout << g.x << ' ' << g.z << '\n';
                
                // we have 3 segments that we might intersect
                T2 ind[3][2] = { {neighbors[cellNo][0], neighbors[cellNo][1]},
                    {neighbors[cellNo][0], neighbors[cellNo][2]},
                    {neighbors[cellNo][1], neighbors[cellNo][2]} };
                
                for ( size_t ns=0; ns<3; ++ns )
                    if ( ind[ns][0]>ind[ns][1] )
                        std::swap( ind[ns][0], ind[ns][1] );
                
                //			std::cout << "\n\nplot(" << curr_pt.x << "," << curr_pt.z << ", 'o')\nhold on\naxis equal\n";
                //			std::cout << "plot([" << curr_pt.x << " " << curr_pt.x+10*g.x << "],["
                //			<< curr_pt.z << " " << curr_pt.z+10*g.z << "], 'r-')\n";
                
                
                sxz<T1> pt_i;
                T1 m1, b1, m2, b2;
                bool foundIntersection = false;
                for ( size_t ns=0; ns<3; ++ns ) {
                    
                    //				std::cout << "\nplot([" << nodes[ ind[ns][0] ].getX() << ' ' << nodes[ ind[ns][1] ].getX() << "], ["
                    //				<< nodes[ ind[ns][0] ].getZ() << ' ' << nodes[ ind[ns][1] ].getZ() << "], '-')\n";
                    
                    // equation of the edge segment
                    T1 den = nodes[ ind[ns][1] ].getX() - nodes[ ind[ns][0] ].getX();
                    
                    if ( den == 0.0 ) {
                        m1 = INFINITY;
                        b1 = nodes[ ind[ns][1] ].getX();
                    } else {
                        m1 = ( nodes[ ind[ns][1] ].getZ() - nodes[ ind[ns][0] ].getZ() ) / den;
                        b1 = nodes[ ind[ns][1] ].getZ() - m1*nodes[ ind[ns][1] ].getX();
                    }
                    
                    // equation of the vector starting at curr_pt & pointing along gradient
                    if ( g.x == 0.0 ) {
                        m2 = INFINITY;
                        b2 = curr_pt.x;
                    } else {
                        m2 = g.z/g.x;
                        b2 = curr_pt.z - m2*curr_pt.x;
                    }
                    
                    if ( onEdge && ind[ns][0]==edgeNodes[0] && ind[ns][1]==edgeNodes[1] ) {
                        
                        //					std::cout << "m: " << m1 << ' ' << m2 << '\n';
                        if ( fabs(m1-m2)<small ) {
                            // curr_pt is on an edge and gradient is along the edge
                            // den is the direction of vector P0->P1 along x
                            if ( boost::math::sign(den) == boost::math::sign(g.x) ) {
                                curr_pt.x = nodes[ ind[ns][1] ].getX();
                                curr_pt.z = nodes[ ind[ns][1] ].getZ();
                                r_data.push_back( curr_pt );
                                foundIntersection = true;
                                break;
                            } else {
                                curr_pt.x = nodes[ ind[ns][0] ].getX();
                                curr_pt.z = nodes[ ind[ns][0] ].getZ();
                                r_data.push_back( curr_pt );
                                foundIntersection = true;
                                break;
                            }
                            
                        }
                        continue;
                    }
                    // intersection of edge segment & gradient vector
                    if ( m1 == INFINITY ) {
                        pt_i.x = b1;
                        pt_i.z = m2*pt_i.x + b2;
                    } else if ( m2 == INFINITY ) {
                        pt_i.x = b2;
                        pt_i.z = m1*pt_i.x + b1;
                    } else {
                        pt_i.x = (b2-b1)/(m1-m2);
                        pt_i.z = m2*pt_i.x + b2;
                    }
                    
                    sxz<T1> vec(pt_i.x-curr_pt.x, pt_i.z-curr_pt.z);
                    if ( dot(vec, g) <= 0.0 ) {
                        // we are not pointing in the same direction
                        continue;
                    }
                    
                    
                    //				std::cout << "plot(" << pt_i.x << "," << pt_i.z << ", 'go')\n";
                    
                    
                    if (((pt_i.x<=nodes[ ind[ns][1] ].getX() && pt_i.x>=nodes[ ind[ns][0] ].getX()) ||
                         (pt_i.x>=nodes[ ind[ns][1] ].getX() && pt_i.x<=nodes[ ind[ns][0] ].getX())) &&
                        ((pt_i.z<=nodes[ ind[ns][0] ].getZ() && pt_i.z>=nodes[ ind[ns][1] ].getZ()) ||
                         (pt_i.z>=nodes[ ind[ns][0] ].getZ() && pt_i.z<=nodes[ ind[ns][1] ].getZ())))
                    {
                        foundIntersection = true;
                        r_data.push_back( pt_i );
                        curr_pt = pt_i;
                        
                        onEdge = true;
                        edgeNodes[0] = ind[ns][0];
                        edgeNodes[1] = ind[ns][1];
                        
                        // find next cell
                        cellNo = findNextCell2(ind[ns][0], ind[ns][1], cellNo);
                        if ( cellNo == std::numeric_limits<T2>::max() ) {
                            std::cout << "\n\nWarning: finding raypath failed to converge for Rx "
                            << Rx.x << ' ' << Rx.z << std::endl;
                            r_data.resize(1);
                            r_data[0] = Rx;
                            reachedTx = true;
                        }
                        break;
                    }
                    
                }
                if ( foundIntersection == false ) {
                    
                    // we must be on an edge with gradient pointing slightly outside triangle
                    sxz<T1> vec(nodes[ edgeNodes[1] ].getX() - nodes[ edgeNodes[0] ].getX(),
                                nodes[ edgeNodes[1] ].getZ() - nodes[ edgeNodes[0] ].getZ());
                    
                    if ( dot(vec, g) > 0.0 ) {
                        curr_pt.x = nodes[ edgeNodes[1] ].getX();
                        curr_pt.z = nodes[ edgeNodes[1] ].getZ();
                        r_data.push_back( curr_pt );
                        foundIntersection = true;
                    } else {
                        curr_pt.x = nodes[ edgeNodes[0] ].getX();
                        curr_pt.z = nodes[ edgeNodes[0] ].getZ();
                        r_data.push_back( curr_pt );
                        foundIntersection = true;
                    }
                }
                
            }
            
            onNode=false;
            for ( T2 nn=0; nn<nodes.size(); ++nn ) {
                if ( nodes[nn] == curr_pt ) {
                    //                std::cout << nodes[nn].getX() << ' ' << nodes[nn].getZ() << '\n';
                    nodeNo = nn;
                    onNode = true;
                    onEdge = false;
                    break;
                }
            }
            
            if ( onNode ) {
                for ( size_t nt=0; nt<Tx.size(); ++nt ) {
                    if ( curr_pt.getDistance( Tx[nt] ) < minDist ) {
                        reachedTx = true;
                        break;
                    }
                }
            } else {
                for ( size_t nt=0; nt<Tx.size(); ++nt ) {
                    if ( txOnNode[nt] ) {
                        for ( auto nc=nodes[txNode[nt]].getOwners().begin();
                             nc!=nodes[txNode[nt]].getOwners().end(); ++nc ) {
                            if ( cellNo == *nc ) {
                                r_data.push_back( Tx[nt] );
                                reachedTx = true;
                                break;
                            }
                        }
                    } else {
                        if ( cellNo == txCell[nt] ) {
                            r_data.push_back( Tx[nt] );
                            reachedTx = true;
                        }
                    }
                    if ( reachedTx ) break;
                }
            }
        }
    }
    
    template<typename T1, typename T2, typename NODE, typename S>
    bool Grid2Duc<T1,T2,NODE,S>::findIntersection(const T2 i0, const T2 i1,
                                                  const sxz<T1> &g,
                                                  sxz<T1> &curr_pt) const {
        
        // equation of the vector starting at curr_pt & pointing along gradient
        T1 m2, b2;
        if ( g.x == 0.0 ) {
            m2 = INFINITY;
            b2 = curr_pt.x;
        } else {
            m2 = g.z/g.x;
            b2 = curr_pt.z - m2*curr_pt.x;
        }
        
        // is gradient direction the same as one of the two edges
        
        // slope of 1st edge segment
        T1 den = nodes[ i0 ].getX() - curr_pt.x;
        
        T1 m1;
        if ( den == 0.0 ) m1 = INFINITY;
        else m1 = ( nodes[ i0 ].getZ() - curr_pt.z ) / den;
        
        if ( m1 == m2 ) {
            curr_pt.x = nodes[ i0 ].getX();
            curr_pt.z = nodes[ i0 ].getZ();
            return true;
        }
        
        // slope of 2nd edge segment
        den = nodes[ i1 ].getX() - curr_pt.x;
        if ( den == 0.0 ) m1 = INFINITY;
        else m1 = ( nodes[ i1 ].getZ() - curr_pt.z ) / den;
        
        if ( m1 == m2 ) {
            curr_pt.x = nodes[ i1 ].getX();
            curr_pt.z = nodes[ i1 ].getZ();
            return true;
        }
        
        // slope of opposing edge segment
        den = nodes[ i1 ].getX() - nodes[ i0 ].getX();
        T1 b1;
        if ( den == 0.0 ) {
            m1 = INFINITY;
            b1 = nodes[ i1 ].getX();
        } else {
            m1 = ( nodes[ i1 ].getZ() - nodes[ i0 ].getZ() ) / den;
            b1 = nodes[ i1 ].getZ() - m1*nodes[ i1 ].getX();
        }
        
        sxz<T1> pt_i;
        // intersection of edge segment & gradient vector
        if ( m1 == INFINITY ) {
            pt_i.x = b1;
            pt_i.z = m2*pt_i.x + b2;
        } else if ( m2 == INFINITY ) {
            pt_i.x = b2;
            pt_i.z = m1*pt_i.x + b1;
        } else {
            pt_i.x = (b2-b1)/(m1-m2);
            pt_i.z = m2*pt_i.x + b2;
        }
        
        curr_pt = pt_i;
        
        return false;
    }
    
    template<typename T1, typename T2, typename NODE, typename S>
    T2 Grid2Duc<T1,T2,NODE,S>::findNextCell1(const T2 i0, const T2 i1, const T2 nodeNo) const {
        std::vector<T2> cells;
        for ( auto nc0=nodes[i0].getOwners().begin(); nc0!=nodes[i0].getOwners().end(); ++nc0 ) {
            if ( std::find(nodes[i1].getOwners().begin(),
                           nodes[i1].getOwners().end(),
                           *nc0) != nodes[i1].getOwners().end()) {
                cells.push_back( *nc0 );
            }
        }
        if ( cells.size() == 1 ) {
            // we are on external edge
            return cells[0];
        }
        for ( auto nc0=nodes[nodeNo].getOwners().begin(); nc0!=nodes[nodeNo].getOwners().end(); ++nc0 ) {
            if ( *nc0 == cells[0] ) {
                return cells[1];
            } else if ( *nc0 == cells[1] ) {
                return cells[0];
            }
        }
        return std::numeric_limits<T2>::max();
    }
    
    template<typename T1, typename T2, typename NODE, typename S>
    T2 Grid2Duc<T1,T2,NODE,S>::findNextCell2(const T2 i0, const T2 i1, const T2 cellNo) const {
        std::vector<T2> cells;
        for ( auto nc0=nodes[i0].getOwners().begin(); nc0!=nodes[i0].getOwners().end(); ++nc0 ) {
            if ( std::find(nodes[i1].getOwners().begin(),
                           nodes[i1].getOwners().end(),
                           *nc0) != nodes[i1].getOwners().end()) {
                cells.push_back( *nc0 );
            }
        }
        if ( cells.size() == 1 ) {
            // we are on external edge
            return cells[0];
        }
        if ( cellNo == cells[0] ) {
            return cells[1];
        } else if ( cellNo == cells[1] ) {
            return cells[0];
        }
        return std::numeric_limits<T2>::max();
    }
    
    template<typename T1, typename T2, typename NODE, typename S>
    void Grid2Duc<T1,T2,NODE,S>::getNeighborNodes(const T2 cellNo,
                                                  std::set<NODE*> &nnodes) const {
        
        for ( size_t n=0; n<3; ++n ) {
            T2 nodeNo = neighbors[cellNo][n];
            nnodes.insert( &(nodes[nodeNo]) );
            
            for ( auto nc=nodes[nodeNo].getOwners().cbegin(); nc!=nodes[nodeNo].getOwners().cend(); ++nc ) {
                for ( size_t nn=0; nn<3; ++nn ) {
                    nnodes.insert( &(nodes[ neighbors[*nc][nn] ]) );
                }
            }
        }
    }
    
}

#endif
