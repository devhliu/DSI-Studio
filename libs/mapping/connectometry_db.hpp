#ifndef CONNECTOMETRY_DB_H
#define CONNECTOMETRY_DB_H
#include <vector>
#include <string>
#include "gzip_interface.hpp"
#include "image/image.hpp"
class fib_data;
class connectometry_db
{
public:
    fib_data* handle;
    std::string subject_report,read_report;
    std::vector<std::string> subject_names;
    unsigned int num_subjects;
    std::vector<float> R2;
    std::vector<const float*> subject_qa;
    unsigned int subject_qa_length;
    std::vector<float> subject_qa_sd;
    image::basic_image<unsigned int,3> vi2si;
    std::vector<unsigned int> si2vi;
    std::vector<std::vector<float> > subject_qa_buf;// merged from other db
public:
    connectometry_db():num_subjects(0){;}
    bool has_db(void)const{return num_subjects > 0;}
    void read_db(fib_data* handle);
    void remove_subject(unsigned int index);
    void calculate_si2vi(void);
    bool sample_odf(gz_mat_read& m,std::vector<float>& data);
    bool sample_index(gz_mat_read& m,std::vector<float>& data,const char* index_name);
    bool is_consistent(gz_mat_read& m);
    bool load_subject_files(const std::vector<std::string>& file_names,
                            const std::vector<std::string>& subject_names_,
                            const char* index_name);
    void get_subject_vector(std::vector<std::vector<float> >& subject_vector,
                            const image::basic_image<int,3>& cerebrum_mask,float fiber_threshold,bool normalize_fp) const;
    void get_subject_vector(unsigned int subject_index,std::vector<float>& subject_vector,
                            const image::basic_image<int,3>& cerebrum_mask,float fiber_threshold,bool normalize_fp) const;
    void get_dif_matrix(std::vector<float>& matrix,const image::basic_image<int,3>& cerebrum_mask,float fiber_threshold,bool normalize_fp);
    void save_subject_vector(const char* output_name,
                             const image::basic_image<int,3>& cerebrum_mask,
                             float fiber_threshold,
                             bool normalize_fp) const;
    void save_subject_data(const char* output_name);
    void get_subject_slice(unsigned int subject_index,unsigned char dim,unsigned int pos,
                            image::basic_image<float,2>& slice) const;
    void get_subject_fa(unsigned int subject_index,std::vector<std::vector<float> >& fa_data) const;
    void get_data_at(unsigned int index,unsigned int fib_index,std::vector<double>& data,bool normalize_qa) const;
    bool get_odf_profile(const char* file_name,std::vector<float>& cur_subject_data);
    bool get_qa_profile(const char* file_name,std::vector<std::vector<float> >& data);
    bool is_db_compatible(const connectometry_db& rhs);
    void read_subject_qa(std::vector<std::vector<float> >&data) const;
    bool add_db(const connectometry_db& rhs);
};



class stat_model{
public:
    image::uniform_dist<int> rand_gen;
    std::mutex  lock_random;
public:
    std::vector<unsigned int> subject_index;
public:
    unsigned int type;
public: // group
    std::vector<int> label;
    unsigned int group1_count,group2_count;
public: // multiple regression
    std::vector<double> X,X_min,X_max,X_range;
    unsigned int feature_count;
    unsigned int study_feature;
    enum {percentage = 0,t = 1,beta = 2,percentile = 3,mean_dif = 4} threshold_type;
    image::multiple_regression<double> mr;
public: // individual
    const float* individual_data;
    float individual_data_sd;
public: // paired
    std::vector<unsigned int> paired;
public:
    stat_model(void):individual_data(0){}
public:
    void init(unsigned int subject_count);
    void remove_subject(unsigned int index);
    void remove_missing_data(double missing_value);
    bool resample(stat_model& rhs,bool null,bool bootstrap);
    bool pre_process(void);
    void select(const std::vector<double>& population,std::vector<double>& selected_population)const;
    double operator()(const std::vector<double>& population,unsigned int pos) const;
    void clear(void)
    {
        label.clear();
        X.clear();
    }
    const stat_model& operator=(const stat_model& rhs)
    {
        subject_index = rhs.subject_index;
        type = rhs.type;
        label = rhs.label;
        group1_count = rhs.group1_count;
        group2_count = rhs.group2_count;
        X = rhs.X;
        X_min = rhs.X_min;
        X_max = rhs.X_max;
        X_range = rhs.X_range;
        feature_count = rhs.feature_count;
        study_feature = rhs.study_feature;
        threshold_type = rhs.threshold_type;
        mr = rhs.mr;
        individual_data = rhs.individual_data;
        paired = rhs.paired;
        return *this;
    }
};



struct connectometry_result{
    std::vector<std::vector<float> > greater,lesser;
    std::vector<const float*> greater_ptr,lesser_ptr;
    void remove_old_index(std::shared_ptr<fib_data> handle);
    bool compare(std::shared_ptr<fib_data> handle,
                 const std::vector<const float*>& fa1,const std::vector<const float*>& fa2,
                 unsigned char normalization);
public:
    std::string report;
    std::string error_msg;
    void initialize(std::shared_ptr<fib_data> fib_file);
    void add_mapping_for_tracking(std::shared_ptr<fib_data> handle,const char* t1,const char* t2);
    bool individual_vs_atlas(std::shared_ptr<fib_data> handle,const char* file_name,unsigned char normalization);
    bool individual_vs_db(std::shared_ptr<fib_data> handle,const char* file_name);
    bool individual_vs_individual(std::shared_ptr<fib_data> handle,
                                  const char* file_name1,const char* file_name2,unsigned char normalization);

};

void calculate_spm(std::shared_ptr<fib_data> handle,connectometry_result& data,stat_model& info,
                   float fiber_threshold,bool normalize_qa,bool& terminated);


#endif // CONNECTOMETRY_DB_H
