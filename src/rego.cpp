//rego: Automatic time series forecasting and missing value imputation.
//
//Copyright (C) Davide Altomare and David Loris <channelattribution.io>
//
//This source code is licensed under the MIT license found in the
//LICENSE file in the root directory of this source tree. 

//#define language_cpp 
//#define language_python
#define language_R

#include <iostream>
#include <vector>
#include <set>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <sstream>
#include <list>
#include <string>
#include <random>
#include <numeric>
#include <time.h> 
#include <thread>
#include <map>
#include <algorithm>
#include <list>
#include <limits> 
#include <functional>
#include <ctime>

#define uli unsigned long int

#define OPTIM_ENABLE_ARMA_WRAPPERS
#ifdef language_py
  #ifdef _WIN32
    #define ARMA_DONT_USE_LAPACK
    #define ARMA_DONT_USE_BLAS
  #endif
#endif
#define ARMA_USE_CXX11
#define ARMA_64BIT_WORD

#include <armadillo>
#include <optim.hpp>

#ifdef language_R
 #define __GXX_EXPERIMENTAL_CXX0X__ 1

 #include <Rcpp.h>
 //#include <RcppArmadillo.h>
 //#define USE_RCPP_ARMADILLO
 
 #ifndef BEGIN_RCPP
 #define BEGIN_RCPP
 #endif
  
 #ifndef END_RCPP
 #define END_RCPP
 #endif
 
 using namespace Rcpp;
#endif


#ifdef language_py
 #include <Python.h>
#endif


using namespace std;
using namespace arma;



//------------------------------------------------------------------------------------------------------------------------
//GENERAL FUNCTIONS
//------------------------------------------------------------------------------------------------------------------------


double lfactorial(uli n)
{
  
 double x;

 x=lgamma(n+1);

 return(x);

} //end function


double lchoose(uli n, uli k)
{
  
  double x;
  
  x=lgamma(n+1)-lgamma(k+1)-lgamma(n-k+1);

  return(x);

} //end function


mat sub_mat(mat* M, vec vr, vec vc)
{
  uli r, c, lvr, lvc;
  lvr=vr.n_elem;
  lvc=vc.n_elem;
  mat Q(lvr,lvc);

  for(c=0; c<lvc; c++){
   for(r=0; r<lvr; r++){ 
    Q(r,c)=(*M)(vr(r),vc(c)); 
   }
  }

  return Q;

} //end function


mat pow_vec(vec* v, vec* w)
{

   uli k, lv; 
   vec vv;

   lv=(*v).n_elem;
   vv=zeros<vec>(lv);

   for(k=0; k<lv; k++){
    vv(k)=pow((*v)(k),(*w)(k));
   }

 return vv;

}

mat Cholesky(mat* M)
{
    double n=(*M).n_rows;
    mat lower(n,n);
 
    // Decomposing a matrix into Lower Triangular
    for (uli i = 0; i < n; i++) {
        for (uli j = 0; j <= i; j++) {
            uli sum = 0;
 
            if (j == i) // summation for diagonals
            {
                for (uli k = 0; k < j; k++){
                    sum += pow(lower(j,k), 2);
                    lower(j,j) = sqrt((*M)(j,j) - sum);
                }
            } else {
 
                // Evaluating L(i, j) using L(j, j)
                for (uli k = 0; k < j; k++){
                    sum += (lower(i,k) * lower(j,k));
                    lower(i,j) = ((*M)(i,j) - sum) / lower(j,j);
                }
            }
        }
    }

    return(lower);
 
}


vec forward_sub(mat* L, vec* b)
{
    /*x = forward_sub(L, b) is the solution to L x = b
       L must be a lower-triangular matrix
       b must be a vector of the same leading dimension as L
    */
    
    double n = (*L).n_cols;
    vec x = zeros<vec>(n);
    double tmp;
    for(double i=0; i<n; ++i){
        tmp = (*b)(i);
         for(double j=0; j<i; ++j){
          tmp -= (*L)(i,j) * x(j);
         }

        x(i) = tmp / (*L)(i,i);
    }
    return(x);
}

vec back_sub(mat* U, vec* b)
{
    /*x = back_sub(U, b) is the solution to U x = b
       U must be an upper-triangular matrix
       b must be a vector of the same leading dimension as U
    */
    
    double n = (*U).n_cols; //it must be double
    vec x = zeros<vec>(n);
    double tmp;
    for(double i=n-1; i>=0; i--){
      tmp = (*b)(i);
       for(uli j=i; j<n; ++j){
        tmp -= (*U)(i,j) * x(j);
        x(i) = tmp / (*U)(i,i);
       }
    }

    return(x);

}

vec solve0(mat* A, vec* b){
 
  mat L=Cholesky(A);
  mat Lt=trans(L);

  vec vy=forward_sub(&L,b);
  vec vx=back_sub(&Lt, &vy);
  
  return(vx);

}

vec solve_linear(mat* A, vec* b){
  
 #ifdef ARMA_DONT_USE_LAPACK
  return(solve0(A,b));
 #else
  return(solve(*A,*b));
 #endif
 

}


//------------------------------------------------------------------------------------------------------------------------
//FUNCTIONS FOR STORING MODELS IN A BINARY TREE
//------------------------------------------------------------------------------------------------------------------------

//this function add a model "M" to "tree"

field<mat> add_to_tree(vec* M, double lM, uli nM, mat* tree, double ltree)
{

 uli j,k;

 field<mat> Res(2,1);

 if(nM==0){
    
  for(j=0; j<=lM; j++){
 
   if((*M)(j)==1){(*tree)(j,0)=j+1;}
   else{(*tree)(j,1)=j+1;}
 
  }
 
  ltree=lM; 
  (*tree).row(ltree)=(*tree).row(ltree)*0+nM;

 }

 uli z,h,iM=0;

 if(nM>0){ //if1
   
  z=0;
  h=ltree+1;
  
  for(j=0; j<=lM; j++){ //for1  
  
   iM=1-(*M)(j);
      
   if(!is_finite((*tree)(z,iM)) && (j<=lM) ){ //if2
     
    (*tree)(z,iM)=h;
     
    for(k=(j+1); k<=lM; k++){ //for2
  
      if((*M)(k)==1){(*tree)(h,0)=h+1;} else{(*tree)(h,1)=h+1;}
      
      h=h+1;
    
    } //end for2    

    iM=1-(*M)(lM);
    ltree=h-1;
    break;
   
   } //end if2
 
   if(j==lM){(*tree)(z,iM)=nM; ltree=ltree+1; break;}
  
   if((*tree)(z,iM)>=0){z=(*tree)(z,iM);}
  
  } //end for1
  

  (*tree)(ltree,iM)=(*tree)(ltree,iM)*0+nM;

 } //end if1


 Res(0,0)=(*tree);
 Res(1,0)=ltree;

 return Res;

} //end function


//this function returns the possible movements from a model "M", given all the previous models visited and stored in "tree"

vec mov_tree(mat* tree, vec* M, uli lM, vec* vlM, uli max_lM)
{

 uli q, k, z, h, iM2;
 double sumM;
 vec mov(lM+1); 
 uvec imov, umov; 
 vec mov2, M2;
 
 
 mov.fill(-1);
 sumM=sum(*M);
 q=0;

 for(k=0; k<=lM; k++){ //for1
  
  M2=(*M); 
  M2(k)=1-(*M)(k);
  z=0;
  
  for(h=0; h<=lM; h++){ //for2
    
   iM2=1-M2(h);
   if(!is_finite((*tree)(z,iM2))){mov(q)=k; q=q+1; break;} else{z=(*tree)(z,iM2);}
   
    } //end for2
  
 
 } //end for1

 imov=find(mov>-1);
 
 if(!imov.is_empty()){
 
  mov=mov.elem(imov);
  umov=conv_to<uvec>::from(mov);

  if(sumM>=max_lM){
  
   mov2=zeros<vec>(lM+1);
   mov2.elem(umov)=ones<vec>(mov.n_elem);
   mov=(mov2%(*M))%(*vlM);
   imov=find(mov>0);
   if(!imov.is_empty()){mov=mov.elem(imov); mov=mov-1;} else{mov=datum::nan;}
  }

 } else {mov=datum::nan;}
 

 return mov;

} //end function



//------------------------------------------------------------------------------------------------------------------------
//FUNCTIONS FOR BAYESIAN STOCHASTIC SEARCH
//------------------------------------------------------------------------------------------------------------------------


double log_H_h_i(double mu, double sigma, double h, double i)
{

 double x;

 x=lfactorial(2*h)+i*log(sigma)-lfactorial(i)+(2*h-2*i)*log(abs(mu))-lfactorial(2*h-2*i);

 return x;

} //end function



double log_FBF_Ga_Gb(vec* G_a, vec* G_b, uli edge, mat* edges, mat* YtY, uli add, double n, double h)
{
  
  uli e1, e2, iwi;
  double i, p, b, S2, mu, sigma, logS2, ilogS2, logHhi, ilog4, log_num1, log_den1, log_w_1, log_num0i0, log_den0i0, log_w_0, log_FBF_unpasso;
  vec V1, V2, G1, V11, pa1, pa0, betah, vv(1), z1;
  uvec iw, ipa1;
  mat e, yty, XtX, invXtX;   
  vec Xty;
  
  e=(*edges).row(edge);
  e1=e(0);
  e2=e(1);

  V1=(*edges).col(0);
  V2=(*edges).col(1);
    
  if(add==1){G1=(*G_a);}else{G1=(*G_b);}   

  V11=(V1+1)%G1;
  iw=find(V2==e2); pa1=V11.elem(iw);
  iw=find(pa1>0); pa1=pa1.elem(iw); pa1=pa1-1;
 
  iw=find(pa1!=e1); if(!iw.is_empty()){pa0=pa1.elem(iw);}else{pa0=datum::nan;}
 
  p=pa1.n_elem;
  b=(p+2*h+1)/n;

  yty=(*YtY)(e2,e2);
    
  // //calcolo w1
   
  vv(0)=e2; Xty=conv_to<vec>::from(sub_mat(YtY,pa1,vv));
  XtX=sub_mat(YtY,pa1,pa1);
  betah=solve_linear(&XtX,&Xty);  

  S2=conv_to<double>::from(yty-(trans(Xty)*betah));
  
  iw=find(pa1==e1); mu=conv_to<double>::from(betah.elem(iw));
  iwi=conv_to<uli>::from(iw); 
  z1=zeros<vec>(pa1.n_elem);
  z1(iwi)=1;
  z1=solve_linear(&XtX,&z1);
  
  sigma=conv_to<double>::from(z1.elem(iw));
  
  if(S2>0){
   log_w_1=(-n*(1-b)/2)*log(datum::pi*b*S2);
   logS2=log(S2);
   log_num1=-datum::inf;
   log_den1=-datum::inf;

   for(i=0; i<=h; i++){
   
    ilogS2=i*logS2;
    logHhi=log_H_h_i(mu,sigma,h,i);
    ilog4=-i*log(4);
      
    log_num1=log_add(log_num1, (ilog4+logHhi+lgamma((n-p-2*i)/2)+ilogS2));
    log_den1=log_add(log_den1, (ilog4+logHhi+lgamma((n*b-p-2*i)/2)+ilogS2));

   }
    
   log_w_1=log_w_1+log_num1-log_den1;
  }else{
   log_w_1=datum::inf;
  }
     
  //calcolo w0

  if(!pa0.is_finite()){p=0;}else{p=pa0.n_elem;}
  
  log_num0i0=lgamma((n-p)/2);
  log_den0i0=lgamma((n*b-p)/2);

  if(p==0){S2=conv_to<double>::from(yty);}
  else{
   vv(0)=e2; Xty=conv_to<vec>::from(sub_mat(YtY,pa0,vv));
   XtX=sub_mat(YtY,pa0,pa0);
   betah=solve_linear(&XtX,&Xty); 

   S2=conv_to<double>::from(yty-(trans(Xty)*betah));
  }

  if(S2>0){
   log_w_0=(-(n*(1-b)/2))*log(datum::pi*b*S2)+log_num0i0-log_den0i0;
  }else{
   log_w_0=datum::inf;
  }
  
  //calcolo FBF

  if(add==1){log_FBF_unpasso=log_w_1-log_w_0;}
  else{log_FBF_unpasso=log_w_0-log_w_1;} 
  
  if(!is_finite(log_FBF_unpasso)){
   log_FBF_unpasso=0;	  
  }

  return log_FBF_unpasso;

} // end function




field<mat> FBF_heart(double nt, mat* YtY, vec* vG_base, double lcv, vec* vlcv, mat* edges, double n_tot_mod, double C, double maxne, double h, bool univariate)
{
    
   uli t, add, edge, imq, limodR, s;
   double ltree, lM, sum_log_FBF, log_FBF_G, log_pi_G, log_num_MP_G, sum_log_RSMP, n_mod_r, log_FBF_t, log_FBF1;
   vec M_log_FBF, log_num_MP, log_sume, G, imod_R, M_log_RSMP, pRSMP, mov, vlM, qh, G_t, M_q, M_P;
   uvec iw;
   mat tree, SM, M_G; 
   field<mat> treeRes, Res(4,1);
   uword i_n_mod_r, imaxe;
 
   M_G=zeros<mat>(lcv,n_tot_mod);  
   M_P=zeros<vec>(n_tot_mod); 
   M_log_FBF=zeros<vec>(n_tot_mod); 
   log_num_MP=zeros<vec>(n_tot_mod); 
   M_q=zeros<vec>(lcv); 
   tree=zeros<mat>(n_tot_mod*lcv,2); tree.fill(datum::nan);
   ltree=datum::nan;
   lM=lcv-1; 
 
   sum_log_FBF=-datum::inf; 
   log_sume=zeros<vec>(lcv); log_sume.fill(-datum::inf);
  
   M_log_RSMP=zeros<vec>(n_tot_mod);
   sum_log_RSMP=-datum::inf;
   imod_R=zeros<vec>(n_tot_mod);

   lM=lcv-1;

   Col<uli> vexit(1);

   for(t=0; t<lcv; t++){ //for1
       
    G=(*vG_base);
    G(t)=1-(*vG_base)(t);
    add=G(t);
    edge=t;

    log_FBF_G=log_FBF_Ga_Gb(&G,vG_base,edge,edges,YtY,add,nt,h);
    
    M_G.col(t)=G;
    
    treeRes=add_to_tree(&G,lM,t,&tree,ltree);
    tree=treeRes(0,0);
    ltree=conv_to<double>::from(treeRes(1,0));
        
    M_log_FBF(t)=log_FBF_G;
    log_pi_G=-log(lcv+1)-lchoose(lcv,sum(G));
    log_num_MP_G=log_FBF_G+log_pi_G;
    log_num_MP(t)=log_num_MP_G;

    sum_log_FBF=log_add(sum_log_FBF, log_num_MP_G);
  
    for(imq=0; imq<lcv; imq++){
     if(G(imq)==1){log_sume(imq)=log_add(log_sume(imq), log_num_MP_G);}
    }
    
    M_q=exp(log_sume-sum_log_FBF);
   
    M_log_RSMP(t)=log_num_MP_G;
    sum_log_RSMP=log_add(sum_log_RSMP, log_num_MP_G);
   
    imod_R(t)=t;
 
   } //end for1

   vec vtmp1,vtmp2;

   if(univariate==0){
   
    limodR=t-1; 
    s=lcv;

    
    while(t<n_tot_mod){ //while1

     pRSMP=exp(M_log_RSMP.subvec(0,limodR)-sum_log_RSMP);
	   pRSMP.max(i_n_mod_r);
     
     n_mod_r=imod_R(i_n_mod_r);
         
     G=M_G.col(n_mod_r);
     G_t=G;
     log_FBF_t=M_log_FBF(n_mod_r);
     
     
     vlM=(*vlcv)+1;
     mov=mov_tree(&tree,&G,lM,&vlM,maxne);
   
     if(!is_finite(mov)){ //if1
      
      imod_R(i_n_mod_r)=-1;
      iw=find(imod_R>-1); imod_R=imod_R.elem(iw);
      M_log_RSMP=M_log_RSMP.elem(iw);
          
      limodR=limodR-1;
      t=t-1;
        
     } else{
        
       vtmp1=(M_q+C)/(1-M_q+C);
       vtmp2=(2*(1-G))-1;
       qh=pow_vec(&vtmp1, &vtmp2);
       qh=qh.elem(conv_to<uvec>::from(mov));
    
        
       if(mov.n_elem==1){ //if2
           
        imod_R(i_n_mod_r)=-1;
        iw=find(imod_R>-1); imod_R=imod_R.elem(iw);
        M_log_RSMP=M_log_RSMP.elem(iw);
      
         limodR=limodR-1;  
         edge=mov(0);
         
        } else{
           
           qh.max(imaxe);
           edge=mov(imaxe);
          
        } // end if2
    
        
        G(edge)=1-G(edge);
        add=G(edge);   
       
        
        log_FBF1=log_FBF_Ga_Gb(&G,&G_t,edge,edges,YtY,add,nt,h);
        log_FBF_G=log_FBF1+log_FBF_t;
      
        M_G.col(t)=G;
        
        treeRes=add_to_tree(&G,lM,t,&tree,ltree);
        tree=treeRes(0,0);
        ltree=conv_to<double>::from(treeRes(1,0));
      
        M_log_FBF(t)=log_FBF_G;
        log_pi_G=-log(lcv+1)-lchoose(lcv,sum(G));
        log_num_MP_G=log_FBF_G+log_pi_G;
        log_num_MP(t)=log_num_MP_G;
    
        sum_log_FBF=log_add(sum_log_FBF, log_num_MP_G);
        
        for(imq=0; imq<lcv; imq++){
         if(G(imq)==1){log_sume(imq)=log_add(log_sume(imq), log_num_MP_G);}
        }
        M_q=exp(log_sume-sum_log_FBF);
	    
        limodR=limodR+1;
        imod_R(limodR)=t;
        M_log_RSMP(limodR)=log_num_MP_G; 
         
     } //end if1
    
     t=t+1;
     s=s+1;
    
    } //end while1 

  }// end if univariate 

  t=t-1; 
  s=s-1;
  
  M_P.subvec(0,t)=exp(log_num_MP.subvec(0,t)-sum_log_FBF);
  if(max(M_P.subvec(0,t))>0){
   M_P.subvec(0,t)=M_P.subvec(0,t)/sum(M_P.subvec(0,t));
  }else{
   M_P.subvec(0,t)=zeros<vec>(t+1);	  	  
  }
  
  M_G=M_G.submat(0,0,lcv-1,t);
  
  Res(0,0)=M_q;
  Res(1,0)=M_G;
  Res(2,0)=M_P;
  Res(3,0)=M_log_FBF.subvec(0,t);

  return Res;
   

} // end function



field<mat> FBF_RS(Mat<double>* Corr_c, double nobs_c, Col<double>* G_base_c, double h_c, double C_c, double n_tot_mod_c, double n_hpp_c, bool univariate)
{

 uli neq, rr; 
 double maxne, Mlogbin_sum, lcv, rrmax, q;
 vec V1, V2, vlcv, vG_base, M_q, M_P, iM_P, M_P2;
 mat edges, G_fin, M_G, M_G2;
 mat YtY;
 field<mat> heartRes;
 
 q=(*Corr_c).n_cols; 

 maxne=nobs_c-2*h_c-2;

 neq=1;

 V1=linspace<vec>(1,q-1,q-1);
 V2=zeros<vec>(q-neq);

 edges=join_rows(V1,V2); 
   
 lcv=V1.n_elem;
 vlcv=linspace<vec>(0,lcv-1,lcv); 
  
 vG_base=flipud(*G_base_c);
   
 rrmax=std::min(maxne,lcv); 
 Mlogbin_sum=0;
   
 for(rr=1; rr<=rrmax; rr++){
  Mlogbin_sum=log_add(Mlogbin_sum,lchoose(lcv,rr));   
 }
 
 n_tot_mod_c=std::min(Mlogbin_sum,log(n_tot_mod_c));
 n_tot_mod_c=round(exp(n_tot_mod_c)); 

 YtY=(*Corr_c)*nobs_c;
 heartRes=FBF_heart(nobs_c, &YtY, &vG_base, lcv, &vlcv, &edges, n_tot_mod_c, C_c, maxne, h_c, univariate);
 
 return(heartRes);

} // end function


void printA(string msg)
{

 #ifdef language_cpp
  cout << msg << endl;
 #endif
 
 #ifdef language_py
  msg="print('" + msg + "')";
  PyRun_SimpleString(msg.c_str());
 #endif

 #ifdef language_R	
  Rcout << msg << endl;
 #endif 
	
}


void xit(){
  vector<int> v;
  printA("execution intentionally interrupted");
  printA(to_string(v[0]));
}

template <typename T>
void printV(vector<T> vec,string name){
  printA(name+": ");
  for (auto i: vec){
    printA(to_string(i));
  }
  printA("");
}

template<typename T>
string vec_to_string (T v, uli len){
    
    // string type0=typeid(v(0)).name();
    // bool flg_string=0;
    // if(type0.find("string")!=string::npos){
    //   flg_string=1;
    // }
    
    string res;
    if(len>0){
     res=to_string(v(0));
     for(uli t=1; t<len; ++t){
       res=res+","+to_string(v(t));
     }
    }else{
     res="empty";
    }
    return(res);
}


template <typename T>
string NumberToString ( T Number )
{
   ostringstream ss;
   ss << Number;
   return ss.str();
}

vector<long int> split_string(const string &s, uli order) {
    
	char delim=' ';
	vector<long int> result(order,-1);
    stringstream ss (s);
    string item;

	uli h=0;
    while (getline (ss, item, delim)) {
		result[h]=stoi(item);
		h=h+1;
    }
		
    return result;
}

template<typename T>
uli find_consecutive_finite(T* x, uli col){
   
   uli max_num=0;
   uli num=0;
   for(uli j=0; j<(*x).n_rows; ++j){
    if(isfinite((*x)(j,col))==1){
     num=num+1;
     if(num>max_num){
       max_num=num;
     }
    }else{
     num=0; 
    } 
   }

  return(max_num);

}

template<typename T>
uli find_consecutive_nan(T* x, uli col){
   
   uli max_num=0;
   uli num=0;
   for(uli j=0; j<(*x).n_rows; ++j){
    if(isfinite((*x)(j,col))==0){
     num=num+1;
     if(num>max_num){
       max_num=num;
     }
    }else{
     num=0; 
    } 
   }

  return(max_num);

}


// template<typename T>
// void save_mat(T* obj,string dir, string name, string type){
//   struct timeval time_now{};
//   gettimeofday(&time_now, nullptr); 
//   time_t msecs_time = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
//   (*obj).save(dir + name + "_" + to_string(msecs_time) + "." + type, csv_ascii);
// }


string f_print_perc(double num){ 
 
 string res;
 if(num>=1){
  res=to_string((double)(floor(num*10000)/100)).substr(0,6);    
 }else if(num>=0.1){ 
  res=to_string((double)(floor(num*10000)/100)).substr(0,5); 
 }else{
  res=to_string((double)(floor(num*10000)/100)).substr(0,4);    	   
 } 
 return(res);
}


vector<string> subvector(vector<string> v, Col<uli> idx){
 vector<string> sub_v;
 for(uli j=0; j<idx.n_rows; ++j){
  sub_v.push_back(v[idx(j)]);
 }
 return(sub_v);
}


vector< vector<double> >  arma_mat_to_std_mat(mat* A) {
    
    vector< vector<double> >  V((*A).n_rows);
    for (size_t i = 0; i < (*A).n_rows; ++i) {
        V[i] = arma::conv_to< vector<double> >::from((*A).row(i));
    };
    
    return V;
}

int rand11(){
 std::random_device rd; 
 std::mt19937 gen(rd()); 
 std::uniform_int_distribution<> distrib(0,  RAND_MAX);
 return(distrib(gen));
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//FUNCTIONS FOR VARIABLE SELECTION AND PREDICTIONS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct str_input
{

  mat MY0;
  mat MY;
  mat corY;
  uli corY_nr;
  uli corY_nc;

  // mat* prt_MY0(){
  //  return(&MY0);
  // }
  
  // mat* prt_MY(){
  //  return(&MY);
  // }

};


str_input data_preparation(mat* Y, Col<uli> vretard)
{

  //varibile target in differenze ritardata
  
  mat MY;
  if(vretard.n_rows>0){
    MY.resize((*Y).n_rows,vretard.n_rows);
      
    for(uli k=0; k<vretard.n_rows; ++k){
     MY.col(k)=shift((*Y).col(0),vretard(k));
     if(vretard(k)>0){
       MY.submat(0,k,vretard(k)-1,k)+=datum::nan;
     }
    }
    //join
    MY=join_rows((*Y),MY);
  }else{
   MY=(*Y);
  }

  //output

  str_input tab_input;
  
  tab_input.MY0=MY;

  MY=MY.rows(find_finite(sum(MY,1)));

  tab_input.MY=MY;

  mat corY=cor(MY);

  tab_input.corY=corY;
  tab_input.corY_nr=MY.n_rows;
  tab_input.corY_nc=MY.n_cols;


  return(tab_input);

}



struct str_output_reg
{
    Col<double> vactual;
    Col<double> vfitted0;
    Col<double> vresid0;
    Col<double> vfitted;
    Col<double> vresid;
    Col<double> vbeta;
    double TSS;
    double RSS;
    double L;
    double L_adj;

    // Col<double>* prt_vresid(){
    //  return(&vresid);
    // }
};

double f_loss_function(const vec& vals_inp, vec* grad_out, void* opt_data)
{
      
  str_input* data = reinterpret_cast<str_input*>(opt_data);
  mat MY=data->MY;

  double err=0;
  double fit=0;

  uli n=MY.n_rows;

  for(uli t=0; t<n; ++t){
    fit=0;
    for(uli k=0; k<vals_inp.n_rows; ++k){
      fit=fit+vals_inp(k)*MY(t,k+1);
    }
    err=err+(abs(MY(t,0)-fit)/n);
  }

  return err;

}

str_output_reg reg(str_input* tab_input, string loss_function)
{
     
    vec vbeta;

    mat X=(*tab_input).MY.cols(1,(*tab_input).MY.n_cols-1);
    vec Y=(*tab_input).MY.col(0);
    mat XtX=trans(X)*X;
    vec Xty=trans(X)*Y;
    vbeta=solve_linear(&XtX,&Xty); 

    if(loss_function=="MAE"){
     optim::de(vbeta,f_loss_function,tab_input);
    }
    //vec vbeta=inv_sympd(XtX)*Xty;
	    
    vec vactual=Y;
    vec vfitted=X*vbeta;
    vec vresid=vactual-vfitted;
	
    vec vactual0=(*tab_input).MY0.col(0);
    vec vfitted0=(*tab_input).MY0.cols(1,(*tab_input).MY0.n_cols-1)*vbeta;
    vec vresid0=vactual0-vfitted0;
     
    str_output_reg Res;

    Res.vbeta=vbeta;
    Res.vactual=Y;
    Res.vfitted0=vfitted0;
    Res.vresid0=vresid0;
    Res.vfitted=vfitted;
    Res.vresid=vresid;

    return(Res);
}


struct str_out_uni_select
{
 Col<uli> vars_x_idx;
 Col<uli> vars_ar_idx; 
 Col<double> logs_FBF;

};

str_out_uni_select model_univariate_selection(mat* Y, Col<uli>* vretard, double max_lag){

  str_out_uni_select str_out;
  
  if((max_lag!=0) | (((*Y).n_cols-1)>0)){
            
    uli from_lag=1;
  
    double cons_rows;
    uli max_lag0;

    cons_rows=(double) find_consecutive_finite(Y,0);

    if(log(cons_rows/10)>0){
     max_lag0=(uli) min(cons_rows/2,cons_rows/log(cons_rows/10));
    }else{
     max_lag0=1; 
    }

    if(max_lag==-1){
     max_lag=(double) max_lag0;
    }else if(max_lag>0){
     max_lag=(double) min(max_lag,(double)max_lag0);
    }
  
    if(max_lag>0){
     (*vretard)=linspace<Col<uli>>(from_lag,(uli)max_lag,(uli)max_lag-from_lag+1);
    }

    double nvars=(*Y).n_cols-1+(*vretard).n_rows;  
    double h_c=1; 
    if(nvars>50){
     h_c=2; 
    } 
    double n_tot_mod_c=nvars*100;
    double C_c=0.01;
    double threshold; 
    
    str_input tab_input;
    field<mat> res;
  
    uli nr,nc,nvar_max;
    Col<double> G_base_c;
    bool univariate=1;
    Col<double> M_q;
    Col<double> M_log_FBF;
  
    //estimate threshold
    
    nc=(*Y).n_cols;
    uvec idfinite=find_finite((*Y).col(0));
    nr=idfinite.n_rows;

    double qt;
    if(nc<10){
      qt=0.90;
    }else{
      qt=0.99;
    }

    uli nv1=(uli)((double)nc);
    uli nv=std::max((uli)200,nv1);
    
    mat Y0 = randu<mat>(nr,nv);

    Col<uli> vretard0;
    tab_input=data_preparation(&Y0,vretard0);
  
    nr=tab_input.corY_nr;
    nc=tab_input.corY_nc;
    nvar_max=(uli)2*std::pow(tab_input.MY.n_rows,0.25);
    G_base_c=zeros<vec>(nc-1);
  
    res=FBF_RS(&tab_input.corY,nr,&G_base_c,h_c,C_c,Y0.n_cols-1+vretard0.n_rows,Y0.n_cols-1+vretard0.n_rows,univariate);
    M_log_FBF=res(3,0); 

    uli nth=(uli) (qt*M_log_FBF.size());
    M_log_FBF=sort(M_log_FBF);
    threshold=M_log_FBF(nth);

    //cout << threshold << endl;

    //univariate  
  
    tab_input=data_preparation(Y,*vretard);
    nr=tab_input.corY_nr;
    nc=tab_input.corY_nc;

    G_base_c=zeros<vec>(nc-1);  
    n_tot_mod_c=(*Y).n_cols-1+(*vretard).n_rows;
    double n_hpp_c=(*Y).n_cols-1+(*vretard).n_rows;

    res=FBF_RS(&tab_input.corY,nr,&G_base_c,h_c,C_c,n_tot_mod_c,n_hpp_c,univariate);

    M_log_FBF=res(3,0);

    uvec ids=find(M_log_FBF>threshold);
    if(ids.n_rows>nvar_max){
     ids = sort_index(M_log_FBF,"descend");
     ids=ids.rows(0,nvar_max-1);
     ids=sort(ids);
    }

    vec rvals=linspace<vec>(0,M_log_FBF.n_rows-1, M_log_FBF.n_rows).elem(ids);

    vec ids_vars=rvals-((*Y).n_cols-1);
    vec ids_vars_ar=ids_vars(find(ids_vars>=0));

    uvec fd_ids_vars=find(ids_vars<0);
    Col<uli> ids_vars_x;
    if(fd_ids_vars.n_cols>0){
     ids_vars_x=conv_to< Col<uli> >::from(rvals(fd_ids_vars));
     ids_vars_x=ids_vars_x+1;
    }
  
	  Col<uli> tmp=(*vretard)(conv_to< uvec >::from(ids_vars_ar));
    str_out.vars_x_idx=ids_vars_x;
    str_out.vars_ar_idx=tmp;
    str_out.logs_FBF=M_log_FBF.elem(ids);
  
    (*vretard)=(*vretard)(conv_to< uvec >::from(ids_vars_ar));
  
  } 
   
  
  return(str_out);

}

struct str_out_multi_select
{
 vec resid; 
 Col<uli> ids_vars_x;
 Col<uli> ids_vars_ar;
 vec vbeta; 
};
  
str_out_multi_select model_multivariate_selection(mat* Y, Col<uli>* ids_vars_x_uni, Col<uli>* vretard, string loss_function){
        
    str_out_multi_select str_out;
    Col<uli> ids_vars_x,v_ar;
    vec vbeta_arx;
    vec vresid((*Y).n_rows,1);

    double nvars=(*Y).n_cols-1+(*vretard).n_rows; 

    if(nvars>0){

      double threshold=0.5;
      double h_c=1; 
      if(nvars>50){
       h_c=2; 
      } 
      double n_tot_mod_c=nvars*100;
      double C_c=0.01;
      double n_hpp_c=1;
       
      mat Y0;
      uvec u_ids_vars_x;
      if((*ids_vars_x_uni).n_rows>0){
       u_ids_vars_x=join_cols(zeros<uvec>(1),conv_to< uvec >::from(*ids_vars_x_uni));
       Y0=(*Y).cols(u_ids_vars_x);
      }else{
       Y0=(*Y).cols(0,0);
      }
    
      str_input tab_input=data_preparation(&Y0,(*vretard));
    
      uli nvar_max=(uli)std::pow(tab_input.MY.n_rows,0.25);
      
      uli nr,nc;
      Col<double> G_base_c;
      bool univariate=0;
      vec M_q;
      field<mat> res;
      uvec ids;
      vec rvals;
  
      uli len_ar=(*vretard).n_rows;
      vec ids_vars;
  
      mat M_G;
      vec M_P;
    
      if((len_ar>0) | (Y0.n_cols>1)){
    	
	      nr=tab_input.corY_nr;
        nc=tab_input.corY_nc;
  
        h_c=1; 
        if((nc-1)>50){
         h_c=2; 
        } 
    
        G_base_c=zeros<vec>(nc-1);
                
        res=FBF_RS(&tab_input.corY,nr,&G_base_c,h_c,C_c,n_tot_mod_c,n_hpp_c,univariate);
                
        M_q=res(0,0);
  
	      ids=find(M_q>=threshold);
	      if(ids.n_rows>nvar_max){
          ids = sort_index(M_q,"descend");
          ids=ids.rows(0,nvar_max-1);
          ids=sort(ids);
        }
    
        rvals=linspace<vec>(0,M_q.n_rows-1, M_q.n_rows).elem(ids);
      
        ids_vars=rvals-(Y0.n_cols-1);
        vec ids_vars_ar=ids_vars(find(ids_vars>=0));
        uvec fd_ids_vars=find(ids_vars<0);
        if(fd_ids_vars.n_cols>0){
         ids_vars_x=conv_to< Col<uli> >::from(rvals(fd_ids_vars));
        }
    
	      if(ids_vars.n_rows>0){
          if(len_ar>0){
            uvec u_ids_vars_ar=conv_to< uvec >::from(ids_vars_ar);
            v_ar=(*vretard).rows(u_ids_vars_ar);
          }
          
          uvec urvals=join_cols(zeros<uvec>(1),conv_to< uvec >::from(rvals+1));
          tab_input.MY0=tab_input.MY0.cols(urvals);
          tab_input.MY=tab_input.MY.cols(urvals);
        
          str_output_reg tab_out_reg=reg(&tab_input, loss_function);
          vbeta_arx=tab_out_reg.vbeta;
          vresid=tab_out_reg.vresid0;
        }else{
	    	  vresid=Y0.col(0);
          double m0=as_scalar(mean(vresid.rows(find_finite(vresid))));
          for(uli t=0; t<vresid.n_rows; ++t){
            vresid(t)=vresid(t)-m0;
	        }
        }
      }else{
    
        vresid=Y0.col(0);
        double m0=as_scalar(mean(vresid.rows(find_finite(vresid))));
        for(uli t=0; t<vresid.n_rows; ++t){
         vresid(t)=vresid(t)-m0;
        }
      
      }

      if(ids_vars_x.size()>0){ 
       u_ids_vars_x=conv_to< uvec >::from(ids_vars_x);
       ids_vars_x=(*ids_vars_x_uni).elem(u_ids_vars_x);
      }
    
    }else{

     vresid=(*Y).col(0);

    }

    str_out.resid=vresid;    
    str_out.ids_vars_x=ids_vars_x;
    str_out.ids_vars_ar=v_ar;
    str_out.vbeta=vbeta_arx;

    return(str_out); 

}



map<string,double> performances(vec vactual, vec vfitted, uli nvars){

  vec vresid=vactual-vfitted;
  uvec non_missing=find_finite(vresid);
  vresid=vresid.elem(non_missing);   
  //double RSS_=as_scalar(sum(pow(vresid,2)));
  
  vactual=vactual.elem(non_missing);
  double m0=mean(vactual);
  // Col<double> vTSS=vactual;
  // for(uli i=0; i<vTSS.n_rows; ++i){
  //  vTSS(i)=vTSS(i)-m0;
  // }
  // vTSS=pow(vTSS,2);
  // double TSS=sum(vTSS);

  // double R2 = 1 - (RSS/TSS);
  // double R2_adj = 1 - (((double)vactual.n_rows-1)/((double)vactual.n_rows-(double)nvars-1))*(RSS/TSS); 
 
  //abs dist


  double L1=as_scalar(sum(abs(vresid)));

  double L0=0;
  for(uli i=0; i<vactual.n_rows; ++i){
   L0=L0+abs(vactual(i)-m0);
  }  

  double L=1-(L1/L0);

  double L_adj=1 - (((double)vactual.n_rows-1)/((double)vactual.n_rows-(double)nvars-1))*(L1/L0);

  map<string,double> res;
  res["L"]=L;
  res["L_adj"]=L_adj;

  return(res);
    
}


struct str_pred_out
{

 mat predictions;
 mat fitted;
 double L;   
 double L_adj;
 
};


str_pred_out sarimax_pred(mat* Y, Col<uli> ids_vars_x, Col<uli> ids_vars_ar, vec vbeta_arx, Col<uli> ids_vars_ma, vec vbeta_ma, bool flg_sim, mat Mfitted, vec probs, uli nsim, string loss_function)
{
    
  str_pred_out str_out;
  vec vresid; 

  uli p=ids_vars_ar.n_rows;
  uli q=ids_vars_ma.n_rows;  
  uli k=vbeta_arx.n_rows-p; //number of regressors

  uli maxpq=0;
  if((p==0) & (q!=0)){
    maxpq=ids_vars_ma.max();
  }else if((p!=0) & (q==0)){
    maxpq=ids_vars_ar.max();
  }else if((p!=0) & (q!=0)){
    maxpq=max(ids_vars_ar.max(),ids_vars_ma.max());
  }
  
  uli npred;
  if(flg_sim==0){
    uli npred1=find_consecutive_nan(Y,0);
    uli npred2=find_consecutive_finite(Y,0)-maxpq-k;

    npred=min(npred1,npred2);
    if(npred==0){
      npred=1;
    }
  }else{
    npred=Mfitted.n_cols;
  }

  
  uli ri;
  double rd;
  // //random_device rdv; 
  mt19937 gen(1234567);
  // default_random_engine gen;
  uniform_real_distribution<double> distrib(0.0,1.0);

  // mt19937 engine(1234567);
  // uniform_real_distribution<double> distrib(0.0,1.0);
  // auto gen = bind(ref(distrib), ref(engine));

  field<vec> Fresid(npred,1);
  vec resid_size(npred);

  if(flg_sim==1){ 
     for(uli kpred=0; kpred<npred; ++kpred){
       vresid=(*Y).col(0)-Mfitted.col(kpred);
       vresid=vresid(find_finite(vresid));
       Fresid(kpred,0)=vresid;
       resid_size(kpred)=(double) vresid.size();
     }
  }else{
    nsim=1;
  }

  vec vbeta_x;
  vec vbeta_ar;

  if(k>0){
   vbeta_x=vbeta_arx.rows(0,k-1);
  }
  if(p>0){
   vbeta_ar=vbeta_arx.rows(k,vbeta_arx.n_rows-1);
  }
  double tar;
  double tma;
  
  mat My_pred((*Y).n_rows, npred);
  vec veps((*Y).n_rows);
  field<mat> Fout(npred,1);
  mat Mout;
  
  if(flg_sim==1){
    for(uli kpred=0; kpred<npred; ++kpred){
     Fout(kpred,0).resize(nsim,(*Y).n_rows);
     Fout(kpred,0).fill(datum::nan);
    }
    }else{  
     for(uli kpred=0; kpred<npred; ++kpred){
      Fout(kpred,0).resize((*Y).n_rows,2);
    }
  }
  
  double pred_x=0;
  double pred_ar=0;
  double pred_ma=0;
  double pred_err;

  uvec ut(1);
  uvec uids_vars_x=conv_to<uvec>::from(ids_vars_x);

  double eps_tma;

  bool is_na_pred_ar=0;


  uli t0, tk, last_kpred;

  for(uli s=0; s<nsim; ++s){

    My_pred.fill(datum::nan);
    veps.fill(datum::nan);

    if(maxpq>0){
     t0=(maxpq+1);
    }else{
     t0=0; 
    }
      

    for(uli t=t0; t<(*Y).n_rows; ++t){
      
      last_kpred=std::min(npred,(uli)(*Y).n_rows-t);
      
      for(uli kpred=0; kpred<last_kpred; ++kpred){ 

        tk=t+kpred;

        ut(0)=tk;
   
        //X
        
        if(k>0){
         pred_x=as_scalar((*Y).submat(ut,uids_vars_x)*vbeta_x);
        }else{
         pred_x=0;
        }
                         
        //AR

        pred_ar=0;
    
        if(p>0){
        
          for(uli p0=0; p0<p; ++p0){
           
           tar=(double)tk-(double)ids_vars_ar(p0);
           
           if(tar>0){
            if((tar<t) & (isfinite((*Y)(tar,0)))){ 
             pred_ar=pred_ar+(*Y)(tar,0)*vbeta_ar(p0);
            }else{ 
             is_na_pred_ar=1;
             for(uli z=0; z<=kpred; ++z){ 
              if(isfinite(My_pred(tar,z))){ 
               pred_ar=pred_ar+My_pred(tar,z)*vbeta_ar(p0);
               is_na_pred_ar=0;
               break;
              }
             }
             if(is_na_pred_ar==1){
               pred_ar=datum::nan;
             }
            }
           }else{
            pred_ar=datum::nan;
            break; 
           }
      
          }
        
        }//end if p
    
        pred_ma=0;

        if(isfinite(pred_ar)){
        
          //MA
      
          if(q>0){
              
            for(uli q0=0; q0<q; ++q0){
        
             tma=(double)tk-(double)ids_vars_ma(q0);
        
             if(tma>0){
              eps_tma=veps(tma);
              if((tma<t) & (isfinite(eps_tma))){
                pred_ma=pred_ma+eps_tma*vbeta_ma(q0);
              }else{
                pred_ma=pred_ma+0;
              }
             }else{
              pred_ma=pred_ma+0;
             }
        
            }
          
          }//end if q

    
        }
            
        if(isfinite(pred_ar)){
         My_pred(tk,kpred)=pred_x+pred_ar+pred_ma; 
        }else{
         My_pred(tk,kpred)=datum::nan; 
        }
            
        if(flg_sim==1){
                    
          rd=distrib(gen);
          ri=(uli)floor(resid_size(kpred)*rd);

          pred_err=Fresid(kpred,0)(ri);

          if(kpred==0){
            if(isfinite((*Y)(t,0))){
              veps(t)=(*Y)(t,0)-pred_x-pred_ar;
            }else{
              veps(t)=pred_ma+pred_err;
            }
          }
          
          if(isfinite(pred_ar)){
           My_pred(tk,kpred)=pred_x+pred_ar+pred_ma+pred_err; 
          }else{
           My_pred(tk,kpred)=datum::nan; 
          }

          Fout(kpred,0)(s,tk)=My_pred(tk,kpred);

        }else{ 

          if(kpred==0){
           if(isfinite((*Y)(t,0))){
            veps(t)=(*Y)(t,0)-pred_x-pred_ar;
           }else{
            veps(t)=pred_ma;
           }
          }
        
        }

      }//end kpred 

    }//end t
  
  }//end s
  
  if(flg_sim==1){
   
   for(uli kpred=0; kpred<npred; ++kpred){ 
     Fout(kpred,0)=quantile(Fout(kpred,0),probs);
     Fout(kpred,0)=join_rows(Mfitted.col(kpred),Fout(kpred,0).t());
     Fout(kpred,0)=join_rows((*Y).col(0),Fout(kpred,0));
   }

   if(npred>1){
      
     //prediction interval correction

     uli kpred=0;
     uli h_low=0, h_up=0;
   
     mat M_low((*Y).n_rows,3); M_low.fill(datum::nan);
     mat M_up((*Y).n_rows,3); M_up.fill(datum::nan);

     mat M0_low((*Y).n_rows,3); M0_low.fill(datum::nan);
     M0_low.col(1)=Fout(0,0).col(2);
     M0_low.col(2).fill(1);
     mat M0_up((*Y).n_rows,3); M0_up.fill(datum::nan);
     M0_up.col(1)=Fout(0,0).col(4);
     M0_up.col(2).fill(1);

     for(uli j=0; j<(*Y).n_rows; ++j){
       if(isfinite((*Y)(j,0))==0){
         if((kpred>0) & (kpred<npred)){
           M_low(h_low,0)=Fout(kpred,0)(j,2);
           M_low(h_low,1)=Fout(0,0)(j,2);
           M_low(h_low,2)=1;
           M0_low(j,0)=Fout(kpred,0)(j,2);
           h_low=h_low+1;
  
           M_up(h_up,0)=Fout(kpred,0)(j,4);
           M_up(h_up,1)=Fout(0,0)(j,4);
           M_up(h_up,2)=1;
           M0_up(j,0)=Fout(kpred,0)(j,4);
           h_up=h_up+1;
         }
         kpred=kpred+1;  
       }else{
        kpred=0; 
       }  
     }

     str_input tab_input_ci;
     str_output_reg reg_ci; 
     vec vlower_bound, vupper_bound;
     vec vquantiles_ci;

     tab_input_ci.MY0=M0_low;
     tab_input_ci.MY=M_low.rows(0,h_low-1);
     reg_ci=reg(&tab_input_ci, loss_function);
     vquantiles_ci=quantile(reg_ci.vresid,probs);
     vlower_bound=reg_ci.vfitted0;
     for(uli j=0; j<vlower_bound.n_rows; ++j){
      vlower_bound(j)=vlower_bound(j)+vquantiles_ci(0);
     }

     tab_input_ci.MY0=M0_up;
     tab_input_ci.MY=M_up.rows(0,h_low-1);
     reg_ci=reg(&tab_input_ci, loss_function);
     vquantiles_ci=quantile(reg_ci.vresid,probs);
     vupper_bound=reg_ci.vfitted0;
     for(uli j=0; j<vupper_bound.n_rows; ++j){
      vupper_bound(j)=vupper_bound(j)+vquantiles_ci(2);
     }  

     Mout=Fout(0,0);

     kpred=0;
     for(uli j=0; j<(*Y).n_rows; ++j){
       if(isfinite((*Y)(j,0))==0){
         if((kpred>0) & (kpred<npred)){
           if((isfinite(vlower_bound(j))) & isfinite(vupper_bound(j))){
            Mout(j,2)=vlower_bound(j);
            Mout(j,4)=vupper_bound(j);
           }
         }
         kpred=kpred+1;  
       }else{
        kpred=0; 
       }  
     }

   }else{

    Mout=Fout(0,0);

   }//end if(npred>1)

  }else{

   uvec nonmiss;

   for(uli kpred=0; kpred<npred; ++kpred){  
     nonmiss=find_finite((*Y).col(0)-My_pred.col(kpred));
     npred=kpred+1;
     if(nonmiss.n_rows<20){ //20 is the number of observations over that the forecasting horizon can be considered significative    
       break;
     }
   }
  
  }
     
  map<string,double> mp_idx_perf;

  double L, L_adj;
  mp_idx_perf=performances((*Y).col(0), My_pred.col(0), (uli)(ids_vars_x.n_rows+ids_vars_ar.n_rows+ids_vars_ma.n_rows));
  L=mp_idx_perf["L"];
  L_adj=mp_idx_perf["L_adj"];

  str_out.predictions=Mout;
  if(flg_sim==0){
   str_out.fitted=My_pred.cols(0,npred-1);
  }else{
   str_out.fitted=My_pred; 
  }
  str_out.L=L;
  str_out.L_adj=L_adj;

  return(str_out);

}


struct str_model_out
{
  Col<uli> ids_vars_x;
  Col<uli> ids_vars_ar;
  Col<double> vbeta_arx;
  Col<uli> ids_vars_ma;
  Col<double> vbeta_ma;
};  


bool CheckVisited(map<vector<uli>,uli>* mv, vector<uli>* v)
{
  bool res=0;
  if ((*mv).find(*v) != (*mv).end()) {
    res=1;
  }
  return(res);
}

pair < pair< vector<str_model_out>, vector<str_pred_out> > , mat > model_selection_prediction(mat* Y, double max_lag, vec probs, uli nsim, string loss_function)
{
          
  pair < pair< vector<str_model_out>, vector<str_pred_out> > , mat > res_out;
  
  str_model_out res_out_i;
  str_pred_out out_pred_i;

  str_out_uni_select out_uni_select_arx;
  str_out_multi_select out_multi_select_arx;

  str_out_uni_select out_uni_select_ma;
  str_out_multi_select out_multi_select_ma;
  
  map<vector<uli>,uli> visited_models;
  
  vec vresid;
  vec vfitted, vfitted_empty;
  mat Mfitted, Mfitted_empty;
  Col<uli> vretard, vretard_empty, vretard1;
  vector<uli> vtmp;
  vector<double> vmin_ids_vars_ar;

  bool flg_x_only=0;
  if(max_lag==0){
   flg_x_only=1; 
  }
  
  Col<uli> id_regressors;
  uvec u_id_regressors;
  
  //SARIX

 
  out_uni_select_arx=model_univariate_selection(Y, &vretard, max_lag);

  // out_uni_select_arx.vars_x_idx.print("out_uni_select_arx.ids_vars_x");
  // out_uni_select_arx.vars_ar_idx.print("out_uni_select_arx.ids_vars_ar");
  // out_uni_select_arx.logs_FBF.print("out_uni_select_arx.logs_FBF");

  if((flg_x_only==0) & (vretard.n_rows>0)){ //if it is a sarimax
     
   //for(double i=-1; i<(double)(vretard.size()-1); ++i){
   // vretard1=vretard;
   // if(i>=0){
   //  vretard1.shed_rows(0,(uli)i);
   // }
   for(double i=0; i<1; ++i){

    vretard1=vretard;

    out_multi_select_arx=model_multivariate_selection(Y, &out_uni_select_arx.vars_x_idx, &vretard1, loss_function);

    vtmp=conv_to< vector<uli> >::from(out_multi_select_arx.ids_vars_ar);   

    if(CheckVisited(&visited_models,&vtmp)==0){
    
       visited_models.insert(pair<vector<uli>,uli>(vtmp,0));

       //MA

       vresid=out_multi_select_arx.resid;
       vretard1.clear();
  
       out_uni_select_ma=model_univariate_selection(&vresid, &vretard1, max_lag);
       out_multi_select_ma=model_multivariate_selection(&vresid, &out_uni_select_ma.vars_x_idx, &vretard1, loss_function);
       
       res_out_i.ids_vars_x=out_multi_select_arx.ids_vars_x;
       res_out_i.ids_vars_ar=out_multi_select_arx.ids_vars_ar;
       res_out_i.vbeta_arx=out_multi_select_arx.vbeta;
       res_out_i.ids_vars_ma=out_multi_select_ma.ids_vars_ar;
       res_out_i.vbeta_ma=out_multi_select_ma.vbeta;

       res_out.first.first.push_back(res_out_i);
  
       if(out_multi_select_arx.ids_vars_ar.n_rows>0){
        vmin_ids_vars_ar.push_back(out_multi_select_arx.ids_vars_ar.min());
       }else{
        vmin_ids_vars_ar.push_back(-1);
       }
       
       out_pred_i=sarimax_pred(Y, out_multi_select_arx.ids_vars_x, out_multi_select_arx.ids_vars_ar, out_multi_select_arx.vbeta, out_multi_select_ma.ids_vars_ar, out_multi_select_ma.vbeta, 0, Mfitted_empty, probs, nsim, loss_function);
       Mfitted=out_pred_i.fitted;

       out_pred_i=sarimax_pred(Y, out_multi_select_arx.ids_vars_x, out_multi_select_arx.ids_vars_ar, out_multi_select_arx.vbeta, out_multi_select_ma.ids_vars_ar, out_multi_select_ma.vbeta, 1, Mfitted, probs, nsim, loss_function);
 
       res_out.first.second.push_back(out_pred_i);

       if(out_multi_select_arx.ids_vars_ar.n_rows==0){
        break; 
       }
  
    }
  
   }//end for

  }else{
   
   flg_x_only=1;

  }

  if(flg_x_only==1){
    
    out_multi_select_arx=model_multivariate_selection(Y, &out_uni_select_arx.vars_x_idx, &vretard_empty, loss_function);

    res_out_i.ids_vars_x=out_multi_select_arx.ids_vars_x;
    res_out_i.ids_vars_ar=out_multi_select_arx.ids_vars_ar;
    res_out_i.vbeta_arx=out_multi_select_arx.vbeta;
    res_out_i.ids_vars_ma=out_multi_select_ma.ids_vars_ar;
    res_out_i.vbeta_ma=out_multi_select_ma.vbeta;

    res_out.first.first.push_back(res_out_i);
    
    out_pred_i=sarimax_pred(Y, out_multi_select_arx.ids_vars_x, out_multi_select_arx.ids_vars_ar, out_multi_select_arx.vbeta, out_multi_select_ma.ids_vars_ar, out_multi_select_ma.vbeta, 0, Mfitted_empty, probs, nsim, loss_function);
    Mfitted=out_pred_i.fitted;
    out_pred_i=sarimax_pred(Y, out_multi_select_arx.ids_vars_x, out_multi_select_arx.ids_vars_ar, out_multi_select_arx.vbeta, out_multi_select_ma.ids_vars_ar, out_multi_select_ma.vbeta, 1, Mfitted, probs, nsim, loss_function);

    res_out.first.second.push_back(out_pred_i);

    res_out.second=out_pred_i.predictions;

  }else{

    uvec idx_tmp;
    uli mod_sel;

    vec vmin_ids_vars_ar_col=conv_to< vec >::from(vmin_ids_vars_ar);
  
    mat final_predictions=res_out.first.second[0].predictions;

    double npred=0;

    for(uli j=0; j<(*Y).n_rows; ++j){
     
     if(j>0){
      if((isfinite((*Y)(j,0))==0) | ((isfinite((*Y)(j,0))==1) & (isfinite((*Y)(j-1,0))==0))){
       npred=npred+1;
      }else{
       npred=0; 
      }
     }else{
      if(isfinite((*Y)(j,0))==0){
       npred=npred+1;
      }else{
       npred=0; 
      }
     } 
     
     if(npred>1)
     {
      
      if(vmin_ids_vars_ar_col(0)==-1){
        mod_sel=0;
      }else{
        idx_tmp=find(vmin_ids_vars_ar_col>=npred);
        if(idx_tmp.n_rows>0){
         mod_sel=(uli) idx_tmp(0);
        }else{
         idx_tmp=find(vmin_ids_vars_ar_col<npred);
         mod_sel=(uli) idx_tmp(idx_tmp.n_rows-1); 
        }
      }
      
      mod_sel=0;
      final_predictions.row(j)=res_out.first.second[mod_sel].predictions.row(j);
     
     }

    }//end for

    res_out.second=final_predictions;
  
  }//end else
  
  return(res_out);


}

struct str_output
{

 mat predictions;
 double L=datum::nan;
 double L_adj=datum::nan;
 
 mat fw_predictions;
 vector<uli> fw_var_x_idx;
 vector<uli> fw_var_ar_idx;
 vector<uli> fw_var_ma_idx;
 double fw_L=datum::nan;   
 double fw_L_adj=datum::nan;

 mat bw_predictions;
 vector<uli> bw_var_x_idx;
 vector<uli> bw_var_ar_idx;
 vector<uli> bw_var_ma_idx;
 double bw_L=datum::nan;
 double bw_L_adj=datum::nan;

};

str_output regpred_cpp(Mat<double>* Y, double max_lag, double alpha, uli nsim, bool flg_print, string direction, string loss_function)
{
      
  str_model_out tab_model;
  str_pred_out tab_pred;
  str_output str_out;
  
  double pinf=(alpha/2);
  double psup=1-(alpha/2);

  vec probs={pinf, 0.5, psup};

  vec vresid_empty;
  vec vresid;
  vec vfitted;

  Col<uli> vtmp_uli;
  map<string,double> mp_idx_perf;

  uli nrows=(*Y).n_rows;
  
  mat Yr;
  if((direction=="<->") | (direction=="<-")){ //do not move below
   Yr=reverse((*Y),0);
  }

  uli bw_k=0, fw_k=0;

  mat predictions, predictions_rev;

  pair < pair< vector<str_model_out>, vector<str_pred_out> > , mat > out_sel_pred;

  if((direction=="<->") | (direction=="->")){ 
                
    //model selection
    if(flg_print==1){
     printA("Forward prediction: making model selection and prediction...");
    }
                
    out_sel_pred=model_selection_prediction(Y, max_lag, probs, nsim, loss_function);

    predictions=out_sel_pred.second;
    
    str_out.fw_predictions=predictions;
    
    str_out.fw_var_x_idx=conv_to< vector<uli> >::from(out_sel_pred.first.first[0].ids_vars_x);
    
    for(uli k=0; k<(uli)out_sel_pred.first.first.size(); ++k){
     vtmp_uli=join_vert(vtmp_uli,out_sel_pred.first.first[k].ids_vars_ar);
    }
    str_out.fw_var_ar_idx=conv_to< vector<uli> >::from(unique(vtmp_uli));

    for(uli k=0; k<(uli)out_sel_pred.first.first.size(); ++k){
     vtmp_uli=join_vert(vtmp_uli,out_sel_pred.first.first[k].ids_vars_ma);
    }
    str_out.fw_var_ma_idx=conv_to< vector<uli> >::from(unique(vtmp_uli));

    fw_k=(uli) (str_out.fw_var_x_idx.size() + str_out.fw_var_ar_idx.size() + str_out.fw_var_ma_idx.size());
    
    mp_idx_perf=performances(predictions.col(0), predictions.col(3), fw_k);

    str_out.fw_L=mp_idx_perf["L"];
    str_out.fw_L_adj=mp_idx_perf["L_adj"];
         
  }
 
  if((direction=="<->") | (direction=="<-")){ 
                    
    //model selection
    if(flg_print==1){
     printA("Backward prediction: making model selection and prediction...");
    }
        
    out_sel_pred=model_selection_prediction(&Yr, max_lag, probs, nsim, loss_function);

    predictions_rev=reverse(out_sel_pred.second);
    
    str_out.bw_predictions=predictions_rev;

    str_out.bw_var_x_idx=conv_to< vector<uli> >::from(out_sel_pred.first.first[0].ids_vars_x);
    
    for(uli k=0; k<(uli)out_sel_pred.first.first.size(); ++k){
     vtmp_uli=join_vert(vtmp_uli,out_sel_pred.first.first[k].ids_vars_ar);
    }
    str_out.bw_var_ar_idx=conv_to< vector<uli> >::from(unique(vtmp_uli));

    for(uli k=0; k<(uli)out_sel_pred.first.first.size(); ++k){
     vtmp_uli=join_vert(vtmp_uli,out_sel_pred.first.first[k].ids_vars_ma);
    }
    str_out.bw_var_ma_idx=conv_to< vector<uli> >::from(unique(vtmp_uli));

    bw_k=(uli) (str_out.bw_var_x_idx.size() + str_out.bw_var_ar_idx.size() + str_out.bw_var_ma_idx.size());

    mp_idx_perf=performances(predictions_rev.col(0), predictions_rev.col(3), bw_k);

    str_out.bw_L=mp_idx_perf["L"];
    str_out.bw_L_adj=mp_idx_perf["L_adj"];

  }

  //collapse
  
  if(direction=="<-"){
    predictions=predictions_rev;
  }

  
  if(direction=="<->"){

   for(uli t=0; t<nrows; ++t){
    for(uli k=1; k<4; ++k){
     if(isfinite(predictions(t,1)) & isfinite(predictions_rev(t,1))){
      predictions(t,1)=(predictions(t,1)+predictions_rev(t,1))/2;
     }else if(isfinite(predictions_rev(t,1))){
      predictions(t,1)=predictions_rev(t,1);
     } 
    }      
   }

  }

  str_out.predictions=predictions;
         
  mp_idx_perf=performances(predictions.col(0), predictions.col(3), max(bw_k,fw_k));

  str_out.L=mp_idx_perf["L"];
  str_out.L_adj=mp_idx_perf["L_adj"];
  
  if(flg_print==1){ 
   printA("Process ended successfully!");
  }
  
  return(str_out);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//FUNCTION FOR PASSING RESULTS TO PYTHON AND R 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mat  std_mat_to_arma_mat(vector< vector<double> >* A) {

    uli nrows=(*A).size();
    uli ncols=(*A)[0].size();
    
    mat V(nrows,ncols);
    
    for (uli j = 0; j < nrows; ++j) {
     for (uli i = 0; i < ncols; ++i) {
       V(j,i)=(*A)[j][i];
     }
    }
   
    return V;

}


#ifdef language_py

pair < pair < list< vector<uli> >, list< vector<string> > > , pair < list< vector< vector<double> > >, list< double> > > 
regpred_py(vector< vector<double> >& Y, double max_lag, double alpha, uli nsim, bool flg_print, string direction, string loss_function)
{

  pair < pair < list< vector<uli> >, list< vector<string> > > , pair < list< vector< vector<double> > >, list< double> > > res;
  mat Y0=std_mat_to_arma_mat(&Y);
  
  str_output str_out=regpred_cpp(&Y0, max_lag, alpha, nsim, flg_print, direction, loss_function);

  res.first.first.push_back(str_out.fw_var_x_idx);
  res.first.first.push_back(str_out.fw_var_ar_idx);
  res.first.first.push_back(str_out.fw_var_ma_idx);
  res.first.first.push_back(str_out.bw_var_x_idx);
  res.first.first.push_back(str_out.bw_var_ar_idx);
  res.first.first.push_back(str_out.bw_var_ma_idx);

  // res.first.second.push_back(str_out.fw_var_x_names);
  // res.first.second.push_back(str_out.bw_var_x_names);

  res.second.first.push_back(arma_mat_to_std_mat(&str_out.predictions));
  res.second.first.push_back(arma_mat_to_std_mat(&str_out.fw_predictions));
  res.second.first.push_back(arma_mat_to_std_mat(&str_out.bw_predictions));
  
  res.second.second.push_back(str_out.L);
  res.second.second.push_back(str_out.L_adj);
  res.second.second.push_back(str_out.fw_L);
  res.second.second.push_back(str_out.fw_L_adj);
  res.second.second.push_back(str_out.bw_L);
  res.second.second.push_back(str_out.bw_L_adj);
  
  return(res);

}

#endif

#ifdef language_R

NumericMatrix  arma_mat_to_num_mat(mat* A) {
    
  NumericMatrix  V((*A).n_rows,(*A).n_cols);
    
	for (size_t j = 0; j < (*A).n_rows; ++j) {
     for (size_t i = 0; i < (*A).n_cols; ++i) {
        V(j,i) =(*A)(j,i);
     }
	};
    
    return V;
}

RcppExport SEXP regpred_R(SEXP Y_p, SEXP max_lag_p, SEXP alpha_p, SEXP nsim_p, SEXP flg_print_p, SEXP direction_p, SEXP loss_function_p)
{

  NumericMatrix Y_0(Y_p); 
  mat Y(Y_0.begin(), Y_0.nrow(), Y_0.ncol(), false);
  
  NumericVector max_lag_0(max_lag_p); 
  double max_lag = Rcpp::as<double>(max_lag_0);
  
  NumericVector alpha_0(alpha_p); 
  double alpha = Rcpp::as<double>(alpha_0);
  
  NumericVector nsim_0(nsim_p); 
  uli nsim = Rcpp::as<uli>(nsim_0);
  
  NumericVector flg_print_0(flg_print_p); 
  bool flg_print = Rcpp::as<bool>(flg_print_0);

  CharacterVector direction_0(direction_p); 
  string direction = Rcpp::as<string>(direction_0);

  CharacterVector loss_function_0(loss_function_p); 
  string loss_function = Rcpp::as<string>(loss_function_0);

  str_output str_out=regpred_cpp(&Y, max_lag, alpha, nsim, flg_print, direction, loss_function);
  
  NumericMatrix predictions=arma_mat_to_num_mat(&str_out.predictions);
  NumericMatrix fw_predictions=arma_mat_to_num_mat(&str_out.fw_predictions);
  NumericMatrix bw_predictions=arma_mat_to_num_mat(&str_out.bw_predictions);

  /*maximum 20 elements admitted for each level*/
  List res=List::create(
    Named("final")=List::create(
      Named("predictions") = predictions,
      Named("L") = str_out.L,
      Named("L_adj") = str_out.L_adj
    ),
    Named("forward")=List::create(
      Named("predictions") = fw_predictions,
      Named("var_x_names") = str_out.fw_var_x_idx, 
      Named("var_ar_idx") = str_out.fw_var_ar_idx, 
      Named("var_ma_idx") = str_out.fw_var_ma_idx,    
      Named("L") = str_out.fw_L,
      Named("L_adj") = str_out.fw_L_adj
    ),
    Named("backward")=List::create( 
      Named("predictions") = bw_predictions,
      Named("var_x_names") = str_out.bw_var_x_idx, 
      Named("var_ar_idx") = str_out.bw_var_ar_idx, 
      Named("var_ma_idx") = str_out.bw_var_ma_idx,    
      Named("L") = str_out.bw_L,
      Named("L_adj") = str_out.bw_L_adj
    )
  );

  return(res);

}

#endif
 
