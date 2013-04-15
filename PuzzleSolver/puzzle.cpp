//
//  puzzle.cpp
//  PuzzleSolver
//
//  Created by Joe Zeimen on 4/5/13.
//  Copyright (c) 2013 Joe Zeimen. All rights reserved.
//

#include "puzzle.h"
#include <opencv/cv.h>
#include "PuzzleDisjointSet.h"
//#include "omp.h"


/*
                   _________      _____
                   \        \    /    /
                    |       /    \   /   _
                 ___/       \____/   |__/ \
                /       PUZZLE SOLVER      }
                \__/\  JOE ___ ZEIMEN  ___/
                     \    /   /       /
                     |    |  |       |
                    /_____/   \_______\
*/



puzzle::puzzle(std::string folderpath){
    pieces = extract_pieces(folderpath);
    solved = false;
}

std::vector<piece> puzzle::extract_pieces(std::string path){
    std::vector<piece> pieces;
    imlist color_images = getImages(path);
    
    //Threshold the image, anything of intensity greater than 45 becomes white (255)
    //anything below becomes 0
    imlist bw = color_to_bw(color_images,45);
    std::cout << "Converted " << bw.size() << " to black and white" << std::endl;
    
    //Filter the noise out of the image
    filter(bw,4);
    
    //For each input image
    for(int i = 0; i<color_images.size(); i++){
        std::vector<std::vector<cv::Point> > contours;
        
        //This isn't used but the opencv function wants it anyways.
        std::vector<cv::Vec4i> hierarchy;
        
        //Need to clone b/c it will get modified
        cv::findContours(bw[i].clone(), contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

        
        //For each contour in that image
        //TODO: Reject contours of very small size to be more resilient to noise
        //TODO: Offset the contour w.r.t. the rectangle and just pass that into
        //    the piece to avoid finding it twice.
        //TODO: (In anticipation of the other TODO's Re-create the b/w image
        //    based off of the contour to eliminate noise in the layer mask
        for(int j = 0; j<contours.size(); j++){
            int bordersize = 10;
            cv::Rect r =  cv::boundingRect(contours[j]);
            r.width += bordersize*2;
            r.height += bordersize*2;
            r.x -= bordersize;
            r.y -= bordersize;
            
            
            cv::Mat mini_color = color_images[i](r);
            cv::Mat mini_bw = bw[i](r);
            //Create a copy so it can't conflict.
            mini_color = mini_color.clone();
            mini_bw = mini_bw.clone();
            
            piece p(mini_color, mini_bw);
            pieces.push_back(p);
            
        }
    }
    
    return pieces;
}




void puzzle::fill_costs(){
    int no_edges = (int) pieces.size()*4;
    
    //TODO: use openmp to speed up this loop w/o blocking the commented lines below
    //don't speed it up as much as it probably should...
//    omp_set_num_threads(4);
//#pragma omp parallel for schedule(dynamic)
    for(int i =0; i<no_edges; i++){
        for(int j=i; j<no_edges; j++){
            match_score score;
            score.edge1 =(int) i;
            score.edge2 =(int) j;
            score.score = pieces[i/4].edges[i%4].compare2(pieces[j/4].edges[j%4]);
//#pragma omp critical
//{
            matches.push_back(score);
//}
        }
    }
    std::sort(matches.begin(),matches.end(),match_score::compare);
}



//Solves the puzzle
void puzzle::solve(){
    
    std::cout << "Finding edge costs..." << std::endl;
    fill_costs();
    std::vector<match_score>::iterator i= matches.begin();
    PuzzleDisjointSet p((int)pieces.size());
    
  
//You can save the individual pieces with their id numbers in the file name
//If the following loop is uncommented.
//    for(int i=0; i<pieces.size(); i++){
//        std::stringstream filename;
//        filename << "/tmp/final/p" << i << ".png";
//        cv::imwrite(filename.str(), pieces[i].full_color);
//    }
    
    while(!p.in_one_set() && i!=matches.end() ){
        int p1 = i->edge1/4;
        int e1 = i->edge1%4;
        int p2 = i->edge2/4;
        int e2 = i->edge2%4;
        
////Uncomment the following lines to spit out pictures of the matched edges...
//        int output_id=0;
//        cv::Mat m = cv::Mat::zeros(500,500,CV_8UC1);
//        std::stringstream out_file_name;
//        out_file_name << "/tmp/final/match" << output_id++ << "_" << p1<< "_" << e1 << "_" <<p2 << "_" <<e2 << ".png";
//        std::vector<std::vector<cv::Point> > contours;
//        contours.push_back(pieces[p1].edges[e1].get_translated_contour(200, 0));
//        contours.push_back(pieces[p2].edges[e2].get_translated_contour_reverse(200, 0));
//        cv::drawContours(m, contours, -1, cv::Scalar(255));
//        std::cout << out_file_name.str() << std::endl;
//        cv::imwrite(out_file_name.str(), m);
        
//        std::cout << "Attempting to merge: " << p1 << " with: " << p2 << " using edges:" << e1 << ", " << e2 << " c:" << i->score <<  std::endl;
        p.join_sets(p1, p2, e1, e2);
        i++;
    }
    
    if(p.in_one_set()){
        std::cout << "Possible solution found" << std::endl;
        solved = true;
        solution = p.get(p.find(1)).locations;
        solution_rotations = p.get(p.find(1)).rotations;
        
        for(int i =0; i<solution.size[0]; i++){
            for(int j=0; j<solution.size[1]; j++){
                int piece_number = solution(i,j);
                pieces[piece_number].rotate(4-solution_rotations(i,j));
            }
        }
        
        
    }
    
    
    
}



//Saves an image of the representation of the puzzle.
//only really works when there are no holes
//TODO: fail when puzzle is in configurations that are not possible i.e. holes
void puzzle::save_image(std::string filepath){
    if(!solved) solve();
    

    //Use get affine to map points...
    int out_image_size = 2150;
    cv::Mat final_out_image(out_image_size,out_image_size,CV_8UC3, cv::Scalar(200,50,3));
    int border = 10;
    
    cv::Point2f ** points = new cv::Point2f*[solution.size[0]+1];
    for(int i = 0; i < solution.size[0]+1; ++i)
        points[i] = new cv::Point2f[solution.size[1]+1];
    
    
    std::cout << "Saving image..." << std::endl;
    for(int i=0; i<solution.size[0];i++){
        for(int j=0; j<solution.size[1]; j++){
            int piece_number = solution(i,j);
    
            float x_dist =(float) cv::norm(pieces[piece_number].get_corner(0)-pieces[piece_number].get_corner(3));
            float y_dist =(float) cv::norm(pieces[piece_number].get_corner(0)-pieces[piece_number].get_corner(1));
            std::vector<cv::Point2f> src;
            std::vector<cv::Point2f> dst;
            
            if(i==0 && j==0){
                points[i][j] = cv::Point2f(border,border);
            }
            if(i==0){
                points[i][j+1] = cv::Point2f(points[i][j].x+border+x_dist,border);
            }
            if(j==0){
                points[i+1][j] = cv::Point2f(border,points[i][j].y+border+y_dist);
            }
            
            dst.push_back(points[i][j]);
            dst.push_back(points[i+1][j]);
            dst.push_back(points[i][j+1]);
            src.push_back(pieces[piece_number].get_corner(0));
            src.push_back(pieces[piece_number].get_corner(1));
            src.push_back(pieces[piece_number].get_corner(3));
            cv::Mat a_trans_mat = cv::estimateRigidTransform(src, dst,true);
            cv::Mat_<double> A = a_trans_mat;
            
            //Lower right corner of each piece
            cv::Point2f l_r_c = pieces[piece_number].get_corner(2);
            //Doing my own matrix multiplication
            points[i+1][j+1] = cv::Point2f((float)(A(0,0)*l_r_c.x+A(0,1)*l_r_c.y+A(0,2)),(float)(A(1,0)*l_r_c.x+A(1,1)*l_r_c.y+A(1,2)));
            
            
            
            cv::Mat layer;
            cv::Mat layer_mask;
            
            int layer_size = out_image_size;
            
            cv::warpAffine(pieces[piece_number].full_color, layer, a_trans_mat, cv::Size2i(layer_size,layer_size));
            cv::warpAffine(pieces[piece_number].bw, layer_mask, a_trans_mat, cv::Size2i(layer_size,layer_size));
            
            layer.copyTo(final_out_image(cv::Rect(0,0,layer_size,layer_size)), layer_mask);
            std::cout << solution(i,j) << ",";
            
        }
        std::cout << std::endl;
    }
    

    cv::imwrite(filepath,final_out_image);
    
    

    for(int i = 0; i < solution.size[0]+1; ++i)
        delete points[i];
    delete points;
    
}
