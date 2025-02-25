#include "ikd_Tree.h"

/*
Description: ikd-Tree: an incremental k-d tree for robotic applications 
Author: Yixi Cai
email: yixicai@connect.hku.hk
*/

/*
Modified by: Eungchang Mason Lee
https://github.com/engcang/ikd-Tree.git
*/

template <typename PointType>
KD_TREE<PointType>::KD_TREE(float delete_param, float balance_param, float box_length)
{
    delete_criterion_param = delete_param;
    balance_criterion_param = balance_param;
    downsample_size = box_length;
    inv_downsample_size = 1.0f / downsample_size;
    Rebuild_Logger.clear();
    termination_flag = false;
    start_thread();
}

template <typename PointType>
KD_TREE<PointType>::~KD_TREE()
{
    stop_thread();
    Delete_Storage_Disabled = true;
    delete_tree_nodes(&Root_Node);
    PointVector().swap(PCL_Storage);
    Rebuild_Logger.clear();
}

template <typename PointType>
void KD_TREE<PointType>::Delete_Ikd_Tree()
{
    delete_tree_nodes(&Root_Node);
    return;
}

template <typename PointType>
void KD_TREE<PointType>::InitializeKDTree(float delete_param, float balance_param, float box_length)
{
    Set_delete_criterion_param(delete_param);
    Set_balance_criterion_param(balance_param);
    set_downsample_param(box_length);
    return;
}

template <typename PointType>
void KD_TREE<PointType>::InitTreeNode(KD_TREE_NODE *root)
{
    root->point.x = 0.0f;
    root->point.y = 0.0f;
    root->point.z = 0.0f;
    root->node_range_x[0] = 0.0f;
    root->node_range_x[1] = 0.0f;
    root->node_range_y[0] = 0.0f;
    root->node_range_y[1] = 0.0f;
    root->node_range_z[0] = 0.0f;
    root->node_range_z[1] = 0.0f;
    root->radius_sq = 0.0f;
    root->division_axis = 0;
    root->father_ptr = nullptr;
    root->left_son_ptr = nullptr;
    root->right_son_ptr = nullptr;
    root->TreeSize = 0;
    root->invalid_point_num = 0;
    root->down_del_num = 0;
    root->point_deleted = false;
    root->tree_deleted = false;
    root->need_push_down_to_left = false;
    root->need_push_down_to_right = false;
    root->point_downsample_deleted = false;
    root->working_flag = false;
    pthread_mutex_init(&(root->push_down_mutex_lock), NULL);
    return;
}

template <typename PointType>
int KD_TREE<PointType>::size()
{
    int s = 0;
    if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
    {
        if (Root_Node != nullptr)
        {
            return Root_Node->TreeSize;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        if (!pthread_mutex_trylock(&working_flag_mutex))
        {
            s = Root_Node->TreeSize;
            pthread_mutex_unlock(&working_flag_mutex);
            return s;
        }
        else
        {
            return Treesize_tmp;
        }
    }
}

template <typename PointType>
BoxPointType KD_TREE<PointType>::tree_range()
{
    BoxPointType range;
    if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
    {
        if (Root_Node != nullptr)
        {
            range.vertex_min[0] = Root_Node->node_range_x[0];
            range.vertex_min[1] = Root_Node->node_range_y[0];
            range.vertex_min[2] = Root_Node->node_range_z[0];
            range.vertex_max[0] = Root_Node->node_range_x[1];
            range.vertex_max[1] = Root_Node->node_range_y[1];
            range.vertex_max[2] = Root_Node->node_range_z[1];
        }
        else
        {
            memset(&range, 0, sizeof(range));
        }
    }
    else
    {
        if (!pthread_mutex_trylock(&working_flag_mutex))
        {
            range.vertex_min[0] = Root_Node->node_range_x[0];
            range.vertex_min[1] = Root_Node->node_range_y[0];
            range.vertex_min[2] = Root_Node->node_range_z[0];
            range.vertex_max[0] = Root_Node->node_range_x[1];
            range.vertex_max[1] = Root_Node->node_range_y[1];
            range.vertex_max[2] = Root_Node->node_range_z[1];
            pthread_mutex_unlock(&working_flag_mutex);
        }
        else
        {
            memset(&range, 0, sizeof(range));
        }
    }
    return range;
}

template <typename PointType>
int KD_TREE<PointType>::validnum()
{
    int s = 0;
    if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
    {
        if (Root_Node != nullptr)
            return (Root_Node->TreeSize - Root_Node->invalid_point_num);
        else
            return 0;
    }
    else
    {
        if (!pthread_mutex_trylock(&working_flag_mutex))
        {
            s = Root_Node->TreeSize - Root_Node->invalid_point_num;
            pthread_mutex_unlock(&working_flag_mutex);
            return s;
        }
        else
        {
            return -1;
        }
    }
}

template <typename PointType>
void KD_TREE<PointType>::root_alpha(float &alpha_bal, float &alpha_del)
{
    if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
    {
        alpha_bal = Root_Node->alpha_bal;
        alpha_del = Root_Node->alpha_del;
        return;
    }
    else
    {
        if (!pthread_mutex_trylock(&working_flag_mutex))
        {
            alpha_bal = Root_Node->alpha_bal;
            alpha_del = Root_Node->alpha_del;
            pthread_mutex_unlock(&working_flag_mutex);
            return;
        }
        else
        {
            alpha_bal = alpha_bal_tmp;
            alpha_del = alpha_del_tmp;
            return;
        }
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::start_thread()
{
    pthread_mutex_init(&termination_flag_mutex_lock, NULL);
    pthread_mutex_init(&rebuild_ptr_mutex_lock, NULL);
    pthread_mutex_init(&rebuild_logger_mutex_lock, NULL);
    pthread_mutex_init(&points_deleted_rebuild_mutex_lock, NULL);
    pthread_mutex_init(&working_flag_mutex, NULL);
    pthread_mutex_init(&search_flag_mutex, NULL);
    pthread_create(&rebuild_thread, NULL, multi_thread_ptr, (void *)this);
    printf("Multi thread started \n");
    return;
}

template <typename PointType>
void KD_TREE<PointType>::stop_thread()
{
    pthread_mutex_lock(&termination_flag_mutex_lock);
    termination_flag = true;
    pthread_mutex_unlock(&termination_flag_mutex_lock);
    if (rebuild_thread)
        pthread_join(rebuild_thread, NULL);
    pthread_mutex_destroy(&termination_flag_mutex_lock);
    pthread_mutex_destroy(&rebuild_logger_mutex_lock);
    pthread_mutex_destroy(&rebuild_ptr_mutex_lock);
    pthread_mutex_destroy(&points_deleted_rebuild_mutex_lock);
    pthread_mutex_destroy(&working_flag_mutex);
    pthread_mutex_destroy(&search_flag_mutex);
    return;
}

template <typename PointType>
void *KD_TREE<PointType>::multi_thread_ptr(void *arg)
{
    KD_TREE *handle = (KD_TREE *)arg;
    handle->multi_thread_rebuild();
    return nullptr;
}

template <typename PointType>
void KD_TREE<PointType>::multi_thread_rebuild()
{
    bool terminated = false;
    KD_TREE_NODE *father_ptr;
    pthread_mutex_lock(&termination_flag_mutex_lock);
    terminated = termination_flag;
    pthread_mutex_unlock(&termination_flag_mutex_lock);
    while (!terminated)
    {
        pthread_mutex_lock(&rebuild_ptr_mutex_lock);
        pthread_mutex_lock(&working_flag_mutex);
        if (Rebuild_Ptr != nullptr)
        {
            /* Traverse and copy */
            if (!Rebuild_Logger.empty())
            {
                printf("\n\n\n\n\n\n\n\n\n\n\n ERROR!!! \n\n\n\n\n\n\n\n\n");
            }
            rebuild_flag = true;
            if (*Rebuild_Ptr == Root_Node)
            {
                Treesize_tmp = Root_Node->TreeSize;
                Validnum_tmp = Root_Node->TreeSize - Root_Node->invalid_point_num;
                alpha_bal_tmp = Root_Node->alpha_bal;
                alpha_del_tmp = Root_Node->alpha_del;
            }
            KD_TREE_NODE *old_root_node = (*Rebuild_Ptr);
            father_ptr = (*Rebuild_Ptr)->father_ptr;
            PointVector().swap(Rebuild_PCL_Storage);
            // Lock Search
            pthread_mutex_lock(&search_flag_mutex);
            while (search_mutex_counter != 0)
            {
                pthread_mutex_unlock(&search_flag_mutex);
                usleep(1);
                pthread_mutex_lock(&search_flag_mutex);
            }
            search_mutex_counter = -1;
            pthread_mutex_unlock(&search_flag_mutex);
            // Lock deleted points cache
            pthread_mutex_lock(&points_deleted_rebuild_mutex_lock);
            flatten(*Rebuild_Ptr, Rebuild_PCL_Storage, MULTI_THREAD_REC);
            // Unlock deleted points cache
            pthread_mutex_unlock(&points_deleted_rebuild_mutex_lock);
            // Unlock Search
            pthread_mutex_lock(&search_flag_mutex);
            search_mutex_counter = 0;
            pthread_mutex_unlock(&search_flag_mutex);
            pthread_mutex_unlock(&working_flag_mutex);
            /* Rebuild and update missed operations*/
            Operation_Logger_Type Operation;
            KD_TREE_NODE *new_root_node = nullptr;
            if (int(Rebuild_PCL_Storage.size()) > 0)
            {
                BuildTree(&new_root_node, 0, Rebuild_PCL_Storage.size() - 1, Rebuild_PCL_Storage);
                // Rebuild has been done. Updates the blocked operations into the new tree
                pthread_mutex_lock(&working_flag_mutex);
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                int tmp_counter = 0;
                while (!Rebuild_Logger.empty())
                {
                    Operation = Rebuild_Logger.front();
                    max_queue_size = std::max(max_queue_size, Rebuild_Logger.size());
                    Rebuild_Logger.pop();
                    pthread_mutex_unlock(&rebuild_logger_mutex_lock);
                    pthread_mutex_unlock(&working_flag_mutex);
                    run_operation(&new_root_node, Operation);
                    tmp_counter++;
                    if (tmp_counter % 10 == 0)
                        usleep(1);
                    pthread_mutex_lock(&working_flag_mutex);
                    pthread_mutex_lock(&rebuild_logger_mutex_lock);
                }
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            /* Replace to original tree*/
            // pthread_mutex_lock(&working_flag_mutex);
            pthread_mutex_lock(&search_flag_mutex);
            while (search_mutex_counter != 0)
            {
                pthread_mutex_unlock(&search_flag_mutex);
                usleep(1);
                pthread_mutex_lock(&search_flag_mutex);
            }
            search_mutex_counter = -1;
            pthread_mutex_unlock(&search_flag_mutex);
            if (father_ptr->left_son_ptr == *Rebuild_Ptr)
            {
                father_ptr->left_son_ptr = new_root_node;
            }
            else if (father_ptr->right_son_ptr == *Rebuild_Ptr)
            {
                father_ptr->right_son_ptr = new_root_node;
            }
            else
            {
                throw "Error: Father ptr incompatible with current node\n";
            }
            if (new_root_node != nullptr)
                new_root_node->father_ptr = father_ptr;
            (*Rebuild_Ptr) = new_root_node;

            if (father_ptr == STATIC_ROOT_NODE)
                Root_Node = STATIC_ROOT_NODE->left_son_ptr;
            KD_TREE_NODE *update_root = *Rebuild_Ptr;
            while (update_root != nullptr && update_root != Root_Node)
            {
                update_root = update_root->father_ptr;
                if (update_root->working_flag)
                    break;
                if (update_root == update_root->father_ptr->left_son_ptr && update_root->father_ptr->need_push_down_to_left)
                    break;
                if (update_root == update_root->father_ptr->right_son_ptr && update_root->father_ptr->need_push_down_to_right)
                    break;
                Update(update_root);
            }
            pthread_mutex_lock(&search_flag_mutex);
            search_mutex_counter = 0;
            pthread_mutex_unlock(&search_flag_mutex);
            Rebuild_Ptr = nullptr;
            pthread_mutex_unlock(&working_flag_mutex);
            rebuild_flag = false;
            /* Delete discarded tree nodes */
            delete_tree_nodes(&old_root_node);
        }
        else
        {
            pthread_mutex_unlock(&working_flag_mutex);
        }
        pthread_mutex_unlock(&rebuild_ptr_mutex_lock);
        pthread_mutex_lock(&termination_flag_mutex_lock);
        terminated = termination_flag;
        pthread_mutex_unlock(&termination_flag_mutex_lock);
        usleep(100);
    }
    printf("Rebuild thread terminated normally\n");
    return;
}

template <typename PointType>
void KD_TREE<PointType>::run_operation(KD_TREE_NODE **root, Operation_Logger_Type operation)
{
    switch (operation.op)
    {
        case ADD_POINT:
            Add_by_point(root, operation.point, false, (*root)->division_axis);
            break;
        case ADD_BOX:
            Add_by_range(root, operation.boxpoint, false);
            break;
        case DELETE_POINT:
            Delete_by_point(root, operation.point, false);
            break;
        case DELETE_BOX:
            Delete_by_range(root, operation.boxpoint, false, false);
            break;
        case DOWNSAMPLE_DELETE:
            Delete_by_range(root, operation.boxpoint, false, true);
            break;
        case PUSH_DOWN:
            (*root)->tree_downsample_deleted |= operation.tree_downsample_deleted;
            (*root)->point_downsample_deleted |= operation.tree_downsample_deleted;
            (*root)->tree_deleted = operation.tree_deleted || (*root)->tree_downsample_deleted;
            (*root)->point_deleted = (*root)->tree_deleted || (*root)->point_downsample_deleted;
            if (operation.tree_downsample_deleted)
                (*root)->down_del_num = (*root)->TreeSize;
            if (operation.tree_deleted)
                (*root)->invalid_point_num = (*root)->TreeSize;
            else
                (*root)->invalid_point_num = (*root)->down_del_num;
            (*root)->need_push_down_to_left = true;
            (*root)->need_push_down_to_right = true;
            break;
        default:
            break;
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Build(PointVector &point_cloud)
{
    if (Root_Node != nullptr)
    {
        delete_tree_nodes(&Root_Node);
    }
    if (point_cloud.size() == 0)
        return;
    STATIC_ROOT_NODE = new KD_TREE_NODE;
    InitTreeNode(STATIC_ROOT_NODE);
    BuildTree(&STATIC_ROOT_NODE->left_son_ptr, 0, point_cloud.size() - 1, point_cloud);
    Update(STATIC_ROOT_NODE);
    STATIC_ROOT_NODE->TreeSize = 0;
    Root_Node = STATIC_ROOT_NODE->left_son_ptr;
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Nearest_Search(const PointType &point, const int &k_nearest, PointVector &Nearest_Points, vector<float> &Point_Distance, const float &max_dist)
{
    MANUAL_HEAP q(2 * k_nearest);
    q.clear();
    vector<float>().swap(Point_Distance);
    if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
    {
        Search(Root_Node, k_nearest, point, q, max_dist);
    }
    else
    {
        pthread_mutex_lock(&search_flag_mutex);
        while (search_mutex_counter == -1)
        {
            pthread_mutex_unlock(&search_flag_mutex);
            usleep(1);
            pthread_mutex_lock(&search_flag_mutex);
        }
        search_mutex_counter += 1;
        pthread_mutex_unlock(&search_flag_mutex);
        Search(Root_Node, k_nearest, point, q, max_dist);
        pthread_mutex_lock(&search_flag_mutex);
        search_mutex_counter -= 1;
        pthread_mutex_unlock(&search_flag_mutex);
    }
    int k_found = std::min(k_nearest, int(q.size()));
    PointVector().swap(Nearest_Points);
    vector<float>().swap(Point_Distance);
    for (int i = 0; i < k_found; i++)
    {
        Nearest_Points.insert(Nearest_Points.begin(), q.top().point);
        Point_Distance.insert(Point_Distance.begin(), q.top().dist);
        q.pop();
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Box_Search(const BoxPointType &Box_of_Point, PointVector &Storage)
{
    Storage.clear();
    Search_by_range(Root_Node, Box_of_Point, Storage);
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Radius_Search(const PointType &point, const float &radius, PointVector &Storage)
{
    Storage.clear();
    Search_by_radius(Root_Node, point, radius, Storage);
    return;
}

template <typename PointType>
bool KD_TREE<PointType>::CollisionLineCheck(const PointType &point1, const PointType &point2, const float &radius)
{
    if (CollisionCheck(point1, radius)) return true;
    if (CollisionCheck(point2, radius)) return true;

    int steps = std::ceil(std::sqrt(calc_dist(point1, point2)) / downsample_size);
    float stepX = (point2.x - point1.x) / steps;
    float stepY = (point2.y - point1.y) / steps;
    float stepZ = (point2.z - point1.z) / steps;
    PointType searchPoint = point1;
    for (int i = 1; i < steps; ++i)
    {
        searchPoint.x += stepX;
        searchPoint.y += stepY;
        searchPoint.z += stepZ;
        if (CollisionCheck(searchPoint, radius))
        {
            return true;
        }
    }

    return false;
}

template <typename PointType>
bool KD_TREE<PointType>::CollisionLineCheckExceptOrigin(const PointType &origin, const PointType &point1, const PointType &point2, const float &radius)
{
    if (CollisionCheck(point1, radius) && !same_point(origin, point1)) return true;
    if (CollisionCheck(point2, radius) && !same_point(origin, point2)) return true;

    int steps = std::ceil(std::sqrt(calc_dist(point1, point2)) / downsample_size);
    float stepX = (point2.x - point1.x) / steps;
    float stepY = (point2.y - point1.y) / steps;
    float stepZ = (point2.z - point1.z) / steps;
    PointType searchPoint = point1;
    for (int i = 1; i < steps; ++i)
    {
        searchPoint.x += stepX;
        searchPoint.y += stepY;
        searchPoint.z += stepZ;
        if (CollisionCheck(searchPoint, radius))
        {
            return true;
        }
    }

    return false;
}

template <typename PointType>
void KD_TREE<PointType>::Ray_Cast(const PointType &pt, const PointType &dir, const float& radius, PointType& hit_point, const float& max_dist)
{
    float dir_length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    float norm_dir_x = dir.x / dir_length;
    float norm_dir_y = dir.y / dir_length;
    float norm_dir_z = dir.z / dir_length;
    
    float stepX = norm_dir_x * downsample_size;
    float stepY = norm_dir_y * downsample_size;
    float stepZ = norm_dir_z * downsample_size;
    int steps = std::ceil(max_dist / downsample_size);
    PointType search_pt = pt;
    
    for (int i = 0; i < steps; ++i)
    {
        if (CollisionCheck(search_pt, radius))
        {
            hit_point = search_pt;
            return;
        }
        search_pt.x += stepX;
        search_pt.y += stepY;
        search_pt.z += stepZ;
    }
    
    hit_point.x = pt.x + norm_dir_x * max_dist;
    hit_point.y = pt.y + norm_dir_y * max_dist;
    hit_point.z = pt.z + norm_dir_z * max_dist;
}

template <typename PointType>
bool KD_TREE<PointType>::CollisionCheck(const PointType &point, const float &radius)
{
    return CollisionCheckRecursive(Root_Node, point, radius);
}

template <typename PointType>
int KD_TREE<PointType>::Add_Points(const PointVector &PointToAdd, const bool &downsample_on)
{
    BoxPointType Box_of_Point;
    PointType mid_point;
    int tmp_counter = 0;

    // If not built, Build first
    if (Root_Node == nullptr)
    {
        PointVector PointToBuild = PointToAdd;
        if (downsample_on) //Input points for Build is also modified to use grid-aligned Downsampling
        {
            PointVector PointToAddDownsampled;
            for (size_t i = 0; i < PointToBuild.size(); ++i)
            {
                mid_point.x = (int(PointToBuild[i].x * inv_downsample_size) - std::signbit(PointToBuild[i].x) + 0.5) * downsample_size;
                mid_point.y = (int(PointToBuild[i].y * inv_downsample_size) - std::signbit(PointToBuild[i].y) + 0.5) * downsample_size;
                mid_point.z = (int(PointToBuild[i].z * inv_downsample_size) - std::signbit(PointToBuild[i].z) + 0.5) * downsample_size;
                // if current mid_point is not already in the vector, push
                if (PointToAddDownsampled.end() == (find_if(PointToAddDownsampled.begin(), PointToAddDownsampled.end(), 
                    [&](PointType pt){return same_point(mid_point, pt);})))
                    PointToAddDownsampled.push_back(mid_point);
            }
            PointToAddDownsampled.swap(PointToBuild);
        }
        Build(PointToBuild);
        return 0;
    }

    // If built, Add points
    for (size_t i = 0; i < PointToAdd.size(); i++)
    {
        if (downsample_on)
        {
            int x_key = int(PointToAdd[i].x * inv_downsample_size) - std::signbit(PointToAdd[i].x);
            int y_key = int(PointToAdd[i].y * inv_downsample_size) - std::signbit(PointToAdd[i].y);
            int z_key = int(PointToAdd[i].z * inv_downsample_size) - std::signbit(PointToAdd[i].z);

            Box_of_Point.vertex_min[0] = x_key * downsample_size;
            Box_of_Point.vertex_max[0] = Box_of_Point.vertex_min[0] + downsample_size;
            Box_of_Point.vertex_min[1] = y_key * downsample_size;
            Box_of_Point.vertex_max[1] = Box_of_Point.vertex_min[1] + downsample_size;
            Box_of_Point.vertex_min[2] = z_key * downsample_size;
            Box_of_Point.vertex_max[2] = Box_of_Point.vertex_min[2] + downsample_size;
            mid_point.x = (x_key + 0.5) * downsample_size;
            mid_point.y = (y_key + 0.5) * downsample_size;
            mid_point.z = (z_key + 0.5) * downsample_size;
            PointVector().swap(Downsample_Storage);
            Search_by_range(Root_Node, Box_of_Point, Downsample_Storage);
            
            if (Downsample_Storage.size() > 0) // a point already exists in the voxel grid, do nothing
                continue;
            else // add a point (not raw, but as the mid point (centroid) of voxel grid)
            {
                if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
                {
                    Add_by_point(&Root_Node, mid_point, true, Root_Node->division_axis);
                    tmp_counter++;
                }
                else
                {
                    Operation_Logger_Type operation;
                    operation.point = mid_point;
                    operation.op = ADD_POINT;
                    pthread_mutex_lock(&working_flag_mutex);
                    Add_by_point(&Root_Node, mid_point, false, Root_Node->division_axis);
                    tmp_counter++;
                    if (rebuild_flag)
                    {
                        pthread_mutex_lock(&rebuild_logger_mutex_lock);
                        Rebuild_Logger.push(operation);
                        pthread_mutex_unlock(&rebuild_logger_mutex_lock);
                    }
                    pthread_mutex_unlock(&working_flag_mutex);
                }
            }
        }
        else
        {
            if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
            {
                Add_by_point(&Root_Node, PointToAdd[i], true, Root_Node->division_axis);
            }
            else
            {
                Operation_Logger_Type operation;
                operation.point = PointToAdd[i];
                operation.op = ADD_POINT;
                pthread_mutex_lock(&working_flag_mutex);
                Add_by_point(&Root_Node, PointToAdd[i], false, Root_Node->division_axis);
                if (rebuild_flag)
                {
                    pthread_mutex_lock(&rebuild_logger_mutex_lock);
                    Rebuild_Logger.push(operation);
                    pthread_mutex_unlock(&rebuild_logger_mutex_lock);
                }
                pthread_mutex_unlock(&working_flag_mutex);
            }
        }
    }
    return tmp_counter;
}

template <typename PointType>
void KD_TREE<PointType>::Add_Point_Boxes(const vector<BoxPointType> &BoxPoints)
{
    for (size_t i = 0; i < BoxPoints.size(); i++)
    {
        if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
        {
            Add_by_range(&Root_Node, BoxPoints[i], true);
        }
        else
        {
            Operation_Logger_Type operation;
            operation.boxpoint = BoxPoints[i];
            operation.op = ADD_BOX;
            pthread_mutex_lock(&working_flag_mutex);
            Add_by_range(&Root_Node, BoxPoints[i], false);
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(operation);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Get_Covered_Points(PointVector &Storage, const bool &get_covered_or_uncovered)
{
    Storage.clear();
    Get_Points_Covered(Root_Node, Storage, get_covered_or_uncovered);
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Get_Points_Covered(KD_TREE_NODE *root, PointVector &Storage, const bool &get_covered_or_uncovered)
{
    if (root == nullptr)
        return;
    Push_Down(root);
    if (!root->point_deleted && (root->point.covered==get_covered_or_uncovered))
    {
        Storage.push_back(root->point);
    }
    Get_Points_Covered(root->left_son_ptr, Storage, get_covered_or_uncovered);
    Get_Points_Covered(root->right_son_ptr, Storage, get_covered_or_uncovered);
    return;
}

template <typename PointType>
int KD_TREE<PointType>::Set_Covered_by_point(KD_TREE_NODE *root, const PointType &point)
{
    if (root == nullptr)
        return -1;
    Push_Down(root);
    if (same_point(root->point, point) && !root->point_deleted)
    {
        root->point.covered = true;
        return 0;
    }
    else
    {
        // if ((root->division_axis == 0 && point.x < root->point.x) || (root->division_axis == 1 && point.y < root->point.y) || (root->division_axis == 2 && point.z < root->point.z))
        // {
        //     if (Set_Covered_by_point(root->left_son_ptr, point) == 0)
        //         return 0;
        // }
        // else
        // {            
        //     if (Set_Covered_by_point(root->right_son_ptr, point) == 0)
        //         return 0;
        // }
        if (Set_Covered_by_point(root->left_son_ptr, point) == 0)
            return 0;
        else if (Set_Covered_by_point(root->right_son_ptr, point) == 0)
            return 0;
    }
    return -1;
}

template <typename PointType>
void KD_TREE<PointType>::Set_Covered_Points(const PointVector &PointsCovered)
{
    for (size_t i = 0; i < PointsCovered.size(); i++)
    {
        if (PointsCovered[i].covered) continue;
        Set_Covered_by_point(Root_Node, PointsCovered[i]);
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Delete_Points(const PointVector &PointToDel)
{
    for (size_t i = 0; i < PointToDel.size(); i++)
    {
        if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
        {
            Delete_by_point(&Root_Node, PointToDel[i], true);
        }
        else
        {
            Operation_Logger_Type operation;
            operation.point = PointToDel[i];
            operation.op = DELETE_POINT;
            pthread_mutex_lock(&working_flag_mutex);
            Delete_by_point(&Root_Node, PointToDel[i], false);
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(operation);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Delete_Points_Accurate(const PointVector &PointToDel)
{
    for (size_t i = 0; i < PointToDel.size(); i++)
    {
        if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
        {
            Delete_by_point_accurate(&Root_Node, PointToDel[i], true);
        }
        else
        {
            Operation_Logger_Type operation;
            operation.point = PointToDel[i];
            operation.op = DELETE_POINT;
            pthread_mutex_lock(&working_flag_mutex);
            Delete_by_point_accurate(&Root_Node, PointToDel[i], false);
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(operation);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    return;
}

template <typename PointType>
int KD_TREE<PointType>::Delete_Point_Boxes(const vector<BoxPointType> &BoxPoints)
{
    int tmp_counter = 0;
    for (size_t i = 0; i < BoxPoints.size(); i++)
    {
        if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
        {
            tmp_counter += Delete_by_range(&Root_Node, BoxPoints[i], true, false);
        }
        else
        {
            Operation_Logger_Type operation;
            operation.boxpoint = BoxPoints[i];
            operation.op = DELETE_BOX;
            pthread_mutex_lock(&working_flag_mutex);
            tmp_counter += Delete_by_range(&Root_Node, BoxPoints[i], false, false);
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(operation);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    return tmp_counter;
}

template <typename PointType>
void KD_TREE<PointType>::Delete_Points_Downsample(const PointVector &PointToDel)
{
    for (size_t i = 0; i < PointToDel.size(); i++)
    {
        BoxPointType Box_of_Point;
        int x_key = int(PointToDel[i].x * inv_downsample_size) - std::signbit(PointToDel[i].x);
        int y_key = int(PointToDel[i].y * inv_downsample_size) - std::signbit(PointToDel[i].y);
        int z_key = int(PointToDel[i].z * inv_downsample_size) - std::signbit(PointToDel[i].z);
        Box_of_Point.vertex_min[0] = x_key * downsample_size;
        Box_of_Point.vertex_max[0] = Box_of_Point.vertex_min[0] + downsample_size;
        Box_of_Point.vertex_min[1] = y_key * downsample_size;
        Box_of_Point.vertex_max[1] = Box_of_Point.vertex_min[1] + downsample_size;
        Box_of_Point.vertex_min[2] = z_key * downsample_size;
        Box_of_Point.vertex_max[2] = Box_of_Point.vertex_min[2] + downsample_size;

        if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != Root_Node)
        {
            Delete_by_range(&Root_Node, Box_of_Point, true, false);
        }
        else
        {
            Operation_Logger_Type operation;
            operation.boxpoint = Box_of_Point;
            operation.op = DELETE_BOX;
            pthread_mutex_lock(&working_flag_mutex);
            Delete_by_range(&Root_Node, Box_of_Point, false, false);
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(operation);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::acquire_removed_points(PointVector &removed_points)
{
    pthread_mutex_lock(&points_deleted_rebuild_mutex_lock);
    for (size_t i = 0; i < Points_deleted.size(); i++)
    {
        removed_points.push_back(Points_deleted[i]);
    }
    for (size_t i = 0; i < Multithread_Points_deleted.size(); i++)
    {
        removed_points.push_back(Multithread_Points_deleted[i]);
    }
    Points_deleted.clear();
    Multithread_Points_deleted.clear();
    pthread_mutex_unlock(&points_deleted_rebuild_mutex_lock);
    return;
}

template <typename PointType>
void KD_TREE<PointType>::BuildTree(KD_TREE_NODE **root, const int &l, const int &r, PointVector &Storage)
{
    if (l > r)
        return;
    *root = new KD_TREE_NODE;
    InitTreeNode(*root);
    int mid = (l + r) >> 1;
    int div_axis = 0;
    int i;
    // Find the best division Axis
    float min_value[3] = {INFINITY, INFINITY, INFINITY};
    float max_value[3] = {-INFINITY, -INFINITY, -INFINITY};
    float dim_range[3] = {0, 0, 0};
    for (i = l; i <= r; i++)
    {
        min_value[0] = std::min(min_value[0], Storage[i].x);
        min_value[1] = std::min(min_value[1], Storage[i].y);
        min_value[2] = std::min(min_value[2], Storage[i].z);
        max_value[0] = std::max(max_value[0], Storage[i].x);
        max_value[1] = std::max(max_value[1], Storage[i].y);
        max_value[2] = std::max(max_value[2], Storage[i].z);
    }
    // Select the longest dimension as division axis
    for (i = 0; i < 3; i++)
        dim_range[i] = max_value[i] - min_value[i];
    for (i = 1; i < 3; i++)
        if (dim_range[i] > dim_range[div_axis])
            div_axis = i;
    // Divide by the division axis and recursively build.

    (*root)->division_axis = div_axis;
    switch (div_axis)
    {
        case 0:
            std::nth_element(begin(Storage) + l, begin(Storage) + mid, begin(Storage) + r + 1, point_cmp_x);
            break;
        case 1:
            std::nth_element(begin(Storage) + l, begin(Storage) + mid, begin(Storage) + r + 1, point_cmp_y);
            break;
        case 2:
            std::nth_element(begin(Storage) + l, begin(Storage) + mid, begin(Storage) + r + 1, point_cmp_z);
            break;
        default:
            std::nth_element(begin(Storage) + l, begin(Storage) + mid, begin(Storage) + r + 1, point_cmp_x);
            break;
    }
    // int split_index = partition(begin(Storage) + mid + 1, begin(Storage) + r + 1, [&](PointType p)
            // {return (Storage[mid].x==p.x && Storage[mid].y==p.y && Storage[mid].z==p.z);}) - begin(Storage) - 1;
    // (*root)->point = Storage[split_index];
    // KD_TREE_NODE *left_son = nullptr, *right_son = nullptr;
    // if (l != split_index)
    // {
    //     int left_son_rightmost_index = 0;
    //     if (split_index != 0)
    //         left_son_rightmost_index = split_index - 1;
    //     BuildTree(&left_son, l, left_son_rightmost_index, Storage);
    //     (*root)->left_son_ptr = left_son;
    // }
    // if (r != split_index)
    // {
    //     BuildTree(&right_son, split_index + 1, r, Storage);
    //     (*root)->right_son_ptr = right_son;
    // }
    (*root)->point = Storage[mid];
    KD_TREE_NODE *left_son = nullptr, *right_son = nullptr;
    BuildTree(&left_son, l, mid - 1, Storage);
    BuildTree(&right_son, mid + 1, r, Storage);
    (*root)->left_son_ptr = left_son;
    (*root)->right_son_ptr = right_son;
    Update((*root));
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Rebuild(KD_TREE_NODE **root)
{
    KD_TREE_NODE *father_ptr;
    if ((*root)->TreeSize >= Multi_Thread_Rebuild_Point_Num)
    {
        if (!pthread_mutex_trylock(&rebuild_ptr_mutex_lock))
        {
            if (Rebuild_Ptr == nullptr || ((*root)->TreeSize > (*Rebuild_Ptr)->TreeSize))
            {
                Rebuild_Ptr = root;
            }
            pthread_mutex_unlock(&rebuild_ptr_mutex_lock);
        }
    }
    else
    {
        father_ptr = (*root)->father_ptr;
        PCL_Storage.clear();
        flatten(*root, PCL_Storage, DELETE_POINTS_REC);
        delete_tree_nodes(root);
        BuildTree(root, 0, PCL_Storage.size() - 1, PCL_Storage);
        if (*root != nullptr)
            (*root)->father_ptr = father_ptr;
        if (*root == Root_Node)
            STATIC_ROOT_NODE->left_son_ptr = *root;
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Reconstruct(PointVector &PointToRecon)
{
    delete_tree_nodes(&Root_Node);
    PointVector().swap(PCL_Storage);
    Rebuild_Logger.clear();
    Build(PointToRecon);
    return;
}

template <typename PointType>
int KD_TREE<PointType>::Delete_by_range(KD_TREE_NODE **root, const BoxPointType &boxpoint, const bool &allow_rebuild, const bool &is_downsample)
{
    if ((*root) == nullptr || (*root)->tree_deleted)
        return 0;
    (*root)->working_flag = true;
    Push_Down(*root);
    int tmp_counter = 0;
    if (boxpoint.vertex_max[0] <= (*root)->node_range_x[0] || boxpoint.vertex_min[0] > (*root)->node_range_x[1])
        return 0;
    if (boxpoint.vertex_max[1] <= (*root)->node_range_y[0] || boxpoint.vertex_min[1] > (*root)->node_range_y[1])
        return 0;
    if (boxpoint.vertex_max[2] <= (*root)->node_range_z[0] || boxpoint.vertex_min[2] > (*root)->node_range_z[1])
        return 0;
    if (boxpoint.vertex_min[0] <= (*root)->node_range_x[0] && boxpoint.vertex_max[0] > (*root)->node_range_x[1] && boxpoint.vertex_min[1] <= (*root)->node_range_y[0] && boxpoint.vertex_max[1] > (*root)->node_range_y[1] && boxpoint.vertex_min[2] <= (*root)->node_range_z[0] && boxpoint.vertex_max[2] > (*root)->node_range_z[1])
    {
        (*root)->tree_deleted = true;
        (*root)->point_deleted = true;
        (*root)->need_push_down_to_left = true;
        (*root)->need_push_down_to_right = true;
        tmp_counter = (*root)->TreeSize - (*root)->invalid_point_num;
        (*root)->invalid_point_num = (*root)->TreeSize;
        if (is_downsample)
        {
            (*root)->tree_downsample_deleted = true;
            (*root)->point_downsample_deleted = true;
            (*root)->down_del_num = (*root)->TreeSize;
        }
        return tmp_counter;
    }
    if (!(*root)->point_deleted && boxpoint.vertex_min[0] <= (*root)->point.x && boxpoint.vertex_max[0] > (*root)->point.x && boxpoint.vertex_min[1] <= (*root)->point.y && boxpoint.vertex_max[1] > (*root)->point.y && boxpoint.vertex_min[2] <= (*root)->point.z && boxpoint.vertex_max[2] > (*root)->point.z)
    {
        (*root)->point_deleted = true;
        tmp_counter += 1;
        if (is_downsample)
            (*root)->point_downsample_deleted = true;
    }
    Operation_Logger_Type delete_box_log;
    if (is_downsample)
        delete_box_log.op = DOWNSAMPLE_DELETE;
    else
        delete_box_log.op = DELETE_BOX;
    delete_box_log.boxpoint = boxpoint;
    if ((Rebuild_Ptr == nullptr) || (*root)->left_son_ptr != *Rebuild_Ptr)
    {
        tmp_counter += Delete_by_range(&((*root)->left_son_ptr), boxpoint, allow_rebuild, is_downsample);
    }
    else
    {
        pthread_mutex_lock(&working_flag_mutex);
        tmp_counter += Delete_by_range(&((*root)->left_son_ptr), boxpoint, false, is_downsample);
        if (rebuild_flag)
        {
            pthread_mutex_lock(&rebuild_logger_mutex_lock);
            Rebuild_Logger.push(delete_box_log);
            pthread_mutex_unlock(&rebuild_logger_mutex_lock);
        }
        pthread_mutex_unlock(&working_flag_mutex);
    }
    if ((Rebuild_Ptr == nullptr) || (*root)->right_son_ptr != *Rebuild_Ptr)
    {
        tmp_counter += Delete_by_range(&((*root)->right_son_ptr), boxpoint, allow_rebuild, is_downsample);
    }
    else
    {
        pthread_mutex_lock(&working_flag_mutex);
        tmp_counter += Delete_by_range(&((*root)->right_son_ptr), boxpoint, false, is_downsample);
        if (rebuild_flag)
        {
            pthread_mutex_lock(&rebuild_logger_mutex_lock);
            Rebuild_Logger.push(delete_box_log);
            pthread_mutex_unlock(&rebuild_logger_mutex_lock);
        }
        pthread_mutex_unlock(&working_flag_mutex);
    }
    Update(*root);
    if (Rebuild_Ptr != nullptr && *Rebuild_Ptr == *root && (*root)->TreeSize < Multi_Thread_Rebuild_Point_Num)
        Rebuild_Ptr = nullptr;
    bool need_rebuild = allow_rebuild & Criterion_Check((*root));
    if (need_rebuild)
        Rebuild(root);
    if ((*root) != nullptr)
        (*root)->working_flag = false;
    return tmp_counter;
}

template <typename PointType>
void KD_TREE<PointType>::Delete_by_point(KD_TREE_NODE **root, const PointType &point, const bool &allow_rebuild)
{
    if ((*root) == nullptr || (*root)->tree_deleted)
        return;
    (*root)->working_flag = true;
    Push_Down(*root);
    if (same_point((*root)->point, point) && !(*root)->point_deleted)
    {
        (*root)->point_deleted = true;
        (*root)->invalid_point_num += 1;
        if ((*root)->invalid_point_num == (*root)->TreeSize)
            (*root)->tree_deleted = true;
        return;
    }
    Operation_Logger_Type delete_log;
    delete_log.op = DELETE_POINT;
    delete_log.point = point;
    if (((*root)->division_axis == 0 && point.x < (*root)->point.x) || ((*root)->division_axis == 1 && point.y < (*root)->point.y) || ((*root)->division_axis == 2 && point.z < (*root)->point.z))
    {
        if ((Rebuild_Ptr == nullptr) || (*root)->left_son_ptr != *Rebuild_Ptr)
        {
            Delete_by_point(&(*root)->left_son_ptr, point, allow_rebuild);
        }
        else
        {
            pthread_mutex_lock(&working_flag_mutex);
            Delete_by_point(&(*root)->left_son_ptr, point, false);
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(delete_log);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    else
    {
        if ((Rebuild_Ptr == nullptr) || (*root)->right_son_ptr != *Rebuild_Ptr)
        {
            Delete_by_point(&(*root)->right_son_ptr, point, allow_rebuild);
        }
        else
        {
            pthread_mutex_lock(&working_flag_mutex);
            Delete_by_point(&(*root)->right_son_ptr, point, false);
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(delete_log);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    Update(*root);
    if (Rebuild_Ptr != nullptr && *Rebuild_Ptr == *root && (*root)->TreeSize < Multi_Thread_Rebuild_Point_Num)
        Rebuild_Ptr = nullptr;
    bool need_rebuild = allow_rebuild & Criterion_Check((*root));
    if (need_rebuild)
        Rebuild(root);
    if ((*root) != nullptr)
        (*root)->working_flag = false;
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Delete_by_point_accurate(KD_TREE_NODE **root, const PointType &point, const bool &allow_rebuild)
{
    if ((*root) == nullptr || (*root)->tree_deleted)
        return;
    (*root)->working_flag = true;
    Push_Down(*root);
    if (same_point((*root)->point, point) && !(*root)->point_deleted)
    {
        (*root)->point_deleted = true;
        (*root)->invalid_point_num += 1;
        if ((*root)->invalid_point_num == (*root)->TreeSize)
            (*root)->tree_deleted = true;
        return;
    }
    Operation_Logger_Type delete_log;
    delete_log.op = DELETE_POINT;
    delete_log.point = point;
    if ((Rebuild_Ptr == nullptr) || (*root)->left_son_ptr != *Rebuild_Ptr)
    {
        Delete_by_point_accurate(&(*root)->left_son_ptr, point, allow_rebuild);
    }
    else
    {
        pthread_mutex_lock(&working_flag_mutex);
        Delete_by_point_accurate(&(*root)->left_son_ptr, point, false);
        if (rebuild_flag)
        {
            pthread_mutex_lock(&rebuild_logger_mutex_lock);
            Rebuild_Logger.push(delete_log);
            pthread_mutex_unlock(&rebuild_logger_mutex_lock);
        }
        pthread_mutex_unlock(&working_flag_mutex);
    }
    if ((Rebuild_Ptr == nullptr) || (*root)->right_son_ptr != *Rebuild_Ptr)
    {
        Delete_by_point_accurate(&(*root)->right_son_ptr, point, allow_rebuild);
    }
    else
    {
        pthread_mutex_lock(&working_flag_mutex);
        Delete_by_point_accurate(&(*root)->right_son_ptr, point, false);
        if (rebuild_flag)
        {
            pthread_mutex_lock(&rebuild_logger_mutex_lock);
            Rebuild_Logger.push(delete_log);
            pthread_mutex_unlock(&rebuild_logger_mutex_lock);
        }
        pthread_mutex_unlock(&working_flag_mutex);
    }
    Update(*root);
    if (Rebuild_Ptr != nullptr && *Rebuild_Ptr == *root && (*root)->TreeSize < Multi_Thread_Rebuild_Point_Num)
        Rebuild_Ptr = nullptr;
    bool need_rebuild = allow_rebuild & Criterion_Check((*root));
    if (need_rebuild)
        Rebuild(root);
    if ((*root) != nullptr)
        (*root)->working_flag = false;
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Add_by_range(KD_TREE_NODE **root, const BoxPointType &boxpoint, const bool &allow_rebuild)
{
    if ((*root) == nullptr)
        return;
    (*root)->working_flag = true;
    Push_Down(*root);
    if (boxpoint.vertex_max[0] <= (*root)->node_range_x[0] || boxpoint.vertex_min[0] > (*root)->node_range_x[1])
        return;
    if (boxpoint.vertex_max[1] <= (*root)->node_range_y[0] || boxpoint.vertex_min[1] > (*root)->node_range_y[1])
        return;
    if (boxpoint.vertex_max[2] <= (*root)->node_range_z[0] || boxpoint.vertex_min[2] > (*root)->node_range_z[1])
        return;
    if (boxpoint.vertex_min[0] <= (*root)->node_range_x[0] && boxpoint.vertex_max[0] > (*root)->node_range_x[1] && boxpoint.vertex_min[1] <= (*root)->node_range_y[0] && boxpoint.vertex_max[1] > (*root)->node_range_y[1] && boxpoint.vertex_min[2] <= (*root)->node_range_z[0] && boxpoint.vertex_max[2] > (*root)->node_range_z[1])
    {
        (*root)->tree_deleted = false || (*root)->tree_downsample_deleted;
        (*root)->point_deleted = false || (*root)->point_downsample_deleted;
        (*root)->need_push_down_to_left = true;
        (*root)->need_push_down_to_right = true;
        (*root)->invalid_point_num = (*root)->down_del_num;
        return;
    }
    if (boxpoint.vertex_min[0] <= (*root)->point.x && boxpoint.vertex_max[0] > (*root)->point.x && boxpoint.vertex_min[1] <= (*root)->point.y && boxpoint.vertex_max[1] > (*root)->point.y && boxpoint.vertex_min[2] <= (*root)->point.z && boxpoint.vertex_max[2] > (*root)->point.z)
    {
        (*root)->point_deleted = (*root)->point_downsample_deleted;
    }
    Operation_Logger_Type add_box_log;
    add_box_log.op = ADD_BOX;
    add_box_log.boxpoint = boxpoint;
    if ((Rebuild_Ptr == nullptr) || (*root)->left_son_ptr != *Rebuild_Ptr)
    {
        Add_by_range(&((*root)->left_son_ptr), boxpoint, allow_rebuild);
    }
    else
    {
        pthread_mutex_lock(&working_flag_mutex);
        Add_by_range(&((*root)->left_son_ptr), boxpoint, false);
        if (rebuild_flag)
        {
            pthread_mutex_lock(&rebuild_logger_mutex_lock);
            Rebuild_Logger.push(add_box_log);
            pthread_mutex_unlock(&rebuild_logger_mutex_lock);
        }
        pthread_mutex_unlock(&working_flag_mutex);
    }
    if ((Rebuild_Ptr == nullptr) || (*root)->right_son_ptr != *Rebuild_Ptr)
    {
        Add_by_range(&((*root)->right_son_ptr), boxpoint, allow_rebuild);
    }
    else
    {
        pthread_mutex_lock(&working_flag_mutex);
        Add_by_range(&((*root)->right_son_ptr), boxpoint, false);
        if (rebuild_flag)
        {
            pthread_mutex_lock(&rebuild_logger_mutex_lock);
            Rebuild_Logger.push(add_box_log);
            pthread_mutex_unlock(&rebuild_logger_mutex_lock);
        }
        pthread_mutex_unlock(&working_flag_mutex);
    }
    Update(*root);
    if (Rebuild_Ptr != nullptr && *Rebuild_Ptr == *root && (*root)->TreeSize < Multi_Thread_Rebuild_Point_Num)
        Rebuild_Ptr = nullptr;
    bool need_rebuild = allow_rebuild & Criterion_Check((*root));
    if (need_rebuild)
        Rebuild(root);
    if ((*root) != nullptr)
        (*root)->working_flag = false;
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Add_by_point(KD_TREE_NODE **root, const PointType &point, const bool &allow_rebuild, const int &father_axis)
{
    if (*root == nullptr)
    {
        *root = new KD_TREE_NODE;
        InitTreeNode(*root);
        (*root)->point = point;
        (*root)->division_axis = (father_axis + 1) % 3;
        Update(*root);
        return;
    }
    (*root)->working_flag = true;
    Operation_Logger_Type add_log;
    add_log.op = ADD_POINT;
    add_log.point = point;
    Push_Down(*root);
    if (((*root)->division_axis == 0 && point.x < (*root)->point.x) || ((*root)->division_axis == 1 && point.y < (*root)->point.y) || ((*root)->division_axis == 2 && point.z < (*root)->point.z))
    {
        if ((Rebuild_Ptr == nullptr) || (*root)->left_son_ptr != *Rebuild_Ptr)
        {
            Add_by_point(&(*root)->left_son_ptr, point, allow_rebuild, (*root)->division_axis);
        }
        else
        {
            pthread_mutex_lock(&working_flag_mutex);
            Add_by_point(&(*root)->left_son_ptr, point, false, (*root)->division_axis);
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(add_log);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    else
    {
        if ((Rebuild_Ptr == nullptr) || (*root)->right_son_ptr != *Rebuild_Ptr)
        {
            Add_by_point(&(*root)->right_son_ptr, point, allow_rebuild, (*root)->division_axis);
        }
        else
        {
            pthread_mutex_lock(&working_flag_mutex);
            Add_by_point(&(*root)->right_son_ptr, point, false, (*root)->division_axis);
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(add_log);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    Update(*root);
    if (Rebuild_Ptr != nullptr && *Rebuild_Ptr == *root && (*root)->TreeSize < Multi_Thread_Rebuild_Point_Num)
        Rebuild_Ptr = nullptr;
    bool need_rebuild = allow_rebuild & Criterion_Check((*root));
    if (need_rebuild)
        Rebuild(root);
    if ((*root) != nullptr)
        (*root)->working_flag = false;
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Search(KD_TREE_NODE *root, const int &k_nearest, const PointType &point, MANUAL_HEAP &q, const float &max_dist)
{
    if (root == nullptr || root->tree_deleted)
        return;
    float cur_dist = calc_box_dist(root, point);
    float max_dist_sqr = max_dist * max_dist;
    if (cur_dist > max_dist_sqr)
        return;
    int retval;
    if (root->need_push_down_to_left || root->need_push_down_to_right)
    {
        retval = pthread_mutex_trylock(&(root->push_down_mutex_lock));
        if (retval == 0)
        {
            Push_Down(root);
            pthread_mutex_unlock(&(root->push_down_mutex_lock));
        }
        else
        {
            pthread_mutex_lock(&(root->push_down_mutex_lock));
            pthread_mutex_unlock(&(root->push_down_mutex_lock));
        }
    }
    if (!root->point_deleted)
    {
        float dist = calc_dist(point, root->point);
        if (dist <= max_dist_sqr && (q.size() < k_nearest || dist < q.top().dist))
        {
            if (q.size() >= k_nearest)
                q.pop();
            PointType_CMP current_point{root->point, dist};
            q.push(current_point);
        }
    }

    float dist_left_node = calc_box_dist(root->left_son_ptr, point);
    float dist_right_node = calc_box_dist(root->right_son_ptr, point);
    if (q.size() < k_nearest || (dist_left_node < q.top().dist && dist_right_node < q.top().dist))
    {
        if (dist_left_node <= dist_right_node)
        {
            if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != root->left_son_ptr)
            {
                Search(root->left_son_ptr, k_nearest, point, q, max_dist);
            }
            else
            {
                pthread_mutex_lock(&search_flag_mutex);
                while (search_mutex_counter == -1)
                {
                    pthread_mutex_unlock(&search_flag_mutex);
                    usleep(1);
                    pthread_mutex_lock(&search_flag_mutex);
                }
                search_mutex_counter += 1;
                pthread_mutex_unlock(&search_flag_mutex);
                Search(root->left_son_ptr, k_nearest, point, q, max_dist);
                pthread_mutex_lock(&search_flag_mutex);
                search_mutex_counter -= 1;
                pthread_mutex_unlock(&search_flag_mutex);
            }
            if (q.size() < k_nearest || dist_right_node < q.top().dist)
            {
                if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != root->right_son_ptr)
                {
                    Search(root->right_son_ptr, k_nearest, point, q, max_dist);
                }
                else
                {
                    pthread_mutex_lock(&search_flag_mutex);
                    while (search_mutex_counter == -1)
                    {
                        pthread_mutex_unlock(&search_flag_mutex);
                        usleep(1);
                        pthread_mutex_lock(&search_flag_mutex);
                    }
                    search_mutex_counter += 1;
                    pthread_mutex_unlock(&search_flag_mutex);
                    Search(root->right_son_ptr, k_nearest, point, q, max_dist);
                    pthread_mutex_lock(&search_flag_mutex);
                    search_mutex_counter -= 1;
                    pthread_mutex_unlock(&search_flag_mutex);
                }
            }
        }
        else
        {
            if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != root->right_son_ptr)
            {
                Search(root->right_son_ptr, k_nearest, point, q, max_dist);
            }
            else
            {
                pthread_mutex_lock(&search_flag_mutex);
                while (search_mutex_counter == -1)
                {
                    pthread_mutex_unlock(&search_flag_mutex);
                    usleep(1);
                    pthread_mutex_lock(&search_flag_mutex);
                }
                search_mutex_counter += 1;
                pthread_mutex_unlock(&search_flag_mutex);
                Search(root->right_son_ptr, k_nearest, point, q, max_dist);
                pthread_mutex_lock(&search_flag_mutex);
                search_mutex_counter -= 1;
                pthread_mutex_unlock(&search_flag_mutex);
            }
            if (q.size() < k_nearest || dist_left_node < q.top().dist)
            {
                if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != root->left_son_ptr)
                {
                    Search(root->left_son_ptr, k_nearest, point, q, max_dist);
                }
                else
                {
                    pthread_mutex_lock(&search_flag_mutex);
                    while (search_mutex_counter == -1)
                    {
                        pthread_mutex_unlock(&search_flag_mutex);
                        usleep(1);
                        pthread_mutex_lock(&search_flag_mutex);
                    }
                    search_mutex_counter += 1;
                    pthread_mutex_unlock(&search_flag_mutex);
                    Search(root->left_son_ptr, k_nearest, point, q, max_dist);
                    pthread_mutex_lock(&search_flag_mutex);
                    search_mutex_counter -= 1;
                    pthread_mutex_unlock(&search_flag_mutex);
                }
            }
        }
    }
    else
    {
        if (dist_left_node < q.top().dist)
        {
            if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != root->left_son_ptr)
            {
                Search(root->left_son_ptr, k_nearest, point, q, max_dist);
            }
            else
            {
                pthread_mutex_lock(&search_flag_mutex);
                while (search_mutex_counter == -1)
                {
                    pthread_mutex_unlock(&search_flag_mutex);
                    usleep(1);
                    pthread_mutex_lock(&search_flag_mutex);
                }
                search_mutex_counter += 1;
                pthread_mutex_unlock(&search_flag_mutex);
                Search(root->left_son_ptr, k_nearest, point, q, max_dist);
                pthread_mutex_lock(&search_flag_mutex);
                search_mutex_counter -= 1;
                pthread_mutex_unlock(&search_flag_mutex);
            }
        }
        if (dist_right_node < q.top().dist)
        {
            if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != root->right_son_ptr)
            {
                Search(root->right_son_ptr, k_nearest, point, q, max_dist);
            }
            else
            {
                pthread_mutex_lock(&search_flag_mutex);
                while (search_mutex_counter == -1)
                {
                    pthread_mutex_unlock(&search_flag_mutex);
                    usleep(1);
                    pthread_mutex_lock(&search_flag_mutex);
                }
                search_mutex_counter += 1;
                pthread_mutex_unlock(&search_flag_mutex);
                Search(root->right_son_ptr, k_nearest, point, q, max_dist);
                pthread_mutex_lock(&search_flag_mutex);
                search_mutex_counter -= 1;
                pthread_mutex_unlock(&search_flag_mutex);
            }
        }
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Search_by_range(KD_TREE_NODE *root, const BoxPointType &boxpoint, PointVector &Storage)
{
    if (root == nullptr)
        return;
    Push_Down(root);
    if (boxpoint.vertex_max[0] <= root->node_range_x[0] || boxpoint.vertex_min[0] > root->node_range_x[1])
        return;
    if (boxpoint.vertex_max[1] <= root->node_range_y[0] || boxpoint.vertex_min[1] > root->node_range_y[1])
        return;
    if (boxpoint.vertex_max[2] <= root->node_range_z[0] || boxpoint.vertex_min[2] > root->node_range_z[1])
        return;
    if (boxpoint.vertex_min[0] <= root->node_range_x[0] && boxpoint.vertex_max[0] > root->node_range_x[1] && boxpoint.vertex_min[1] <= root->node_range_y[0] && boxpoint.vertex_max[1] > root->node_range_y[1] && boxpoint.vertex_min[2] <= root->node_range_z[0] && boxpoint.vertex_max[2] > root->node_range_z[1])
    {
        flatten(root, Storage, NOT_RECORD);
        return;
    }
    if (boxpoint.vertex_min[0] <= root->point.x && boxpoint.vertex_max[0] > root->point.x && boxpoint.vertex_min[1] <= root->point.y && boxpoint.vertex_max[1] > root->point.y && boxpoint.vertex_min[2] <= root->point.z && boxpoint.vertex_max[2] > root->point.z)
    {
        if (!root->point_deleted)
            Storage.push_back(root->point);
    }
    if ((Rebuild_Ptr == nullptr) || root->left_son_ptr != *Rebuild_Ptr)
    {
        Search_by_range(root->left_son_ptr, boxpoint, Storage);
    }
    else
    {
        pthread_mutex_lock(&search_flag_mutex);
        Search_by_range(root->left_son_ptr, boxpoint, Storage);
        pthread_mutex_unlock(&search_flag_mutex);
    }
    if ((Rebuild_Ptr == nullptr) || root->right_son_ptr != *Rebuild_Ptr)
    {
        Search_by_range(root->right_son_ptr, boxpoint, Storage);
    }
    else
    {
        pthread_mutex_lock(&search_flag_mutex);
        Search_by_range(root->right_son_ptr, boxpoint, Storage);
        pthread_mutex_unlock(&search_flag_mutex);
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Search_by_radius(KD_TREE_NODE *root, const PointType &point, const float &radius, PointVector &Storage)
{
    if (root == nullptr)
        return;
    Push_Down(root);
    PointType range_center;
    range_center.x = (root->node_range_x[0] + root->node_range_x[1]) * 0.5;
    range_center.y = (root->node_range_y[0] + root->node_range_y[1]) * 0.5;
    range_center.z = (root->node_range_z[0] + root->node_range_z[1]) * 0.5;
    float dist = std::sqrt(calc_dist(range_center, point));
    if (dist > radius + std::sqrt(root->radius_sq)) return;
    if (dist <= radius - std::sqrt(root->radius_sq)) 
    {
        flatten(root, Storage, NOT_RECORD);
        return;
    }
    if (!root->point_deleted && calc_dist(root->point, point) <= radius * radius){
        Storage.push_back(root->point);
    }
    if ((Rebuild_Ptr == nullptr) || root->left_son_ptr != *Rebuild_Ptr)
    {
        Search_by_radius(root->left_son_ptr, point, radius, Storage);
    }
    else
    {
        pthread_mutex_lock(&search_flag_mutex);
        Search_by_radius(root->left_son_ptr, point, radius, Storage);
        pthread_mutex_unlock(&search_flag_mutex);
    }
    if ((Rebuild_Ptr == nullptr) || root->right_son_ptr != *Rebuild_Ptr)
    {
        Search_by_radius(root->right_son_ptr, point, radius, Storage);
    }
    else
    {
        pthread_mutex_lock(&search_flag_mutex);
        Search_by_radius(root->right_son_ptr, point, radius, Storage);
        pthread_mutex_unlock(&search_flag_mutex);
    }    
    return;
}

template <typename PointType>
bool KD_TREE<PointType>::CollisionCheckRecursive(KD_TREE_NODE *root, const PointType &point, const float &radius)
{
    if (root == nullptr) return false;
    Push_Down(root);
    PointType range_center;
    range_center.x = (root->node_range_x[0] + root->node_range_x[1]) * 0.5;
    range_center.y = (root->node_range_y[0] + root->node_range_y[1]) * 0.5;
    range_center.z = (root->node_range_z[0] + root->node_range_z[1]) * 0.5;
    float dist = std::sqrt(calc_dist(range_center, point));
    if (dist > radius + std::sqrt(root->radius_sq)) return false;
    if (dist <= radius - std::sqrt(root->radius_sq)) 
    {
        return true;
    }
    if (!root->point_deleted && calc_dist(root->point, point) <= radius * radius){
        return true;
    }
    if ((Rebuild_Ptr == nullptr) || root->left_son_ptr != *Rebuild_Ptr)
    {
        if (CollisionCheckRecursive(root->left_son_ptr, point, radius))
        {
            return true;
        }
    }
    else
    {
        pthread_mutex_lock(&search_flag_mutex);
        bool collision_ = CollisionCheckRecursive(root->left_son_ptr, point, radius);
        pthread_mutex_unlock(&search_flag_mutex);
        if (collision_)
        {
            return true;
        }
    }
    if ((Rebuild_Ptr == nullptr) || root->right_son_ptr != *Rebuild_Ptr)
    {
        if (CollisionCheckRecursive(root->right_son_ptr, point, radius))
        {
            return true;
        }
    }
    else
    {
        pthread_mutex_lock(&search_flag_mutex);
        bool collision_ = CollisionCheckRecursive(root->right_son_ptr, point, radius);
        pthread_mutex_unlock(&search_flag_mutex);
        if (collision_)
        {
            return true;
        }
    }    
    return false;
}

template <typename PointType>
bool KD_TREE<PointType>::Criterion_Check(KD_TREE_NODE *root)
{
    if (root->TreeSize <= Minimal_Unbalanced_Tree_Size)
    {
        return false;
    }
    float balance_evaluation = 0.0f;
    float delete_evaluation = 0.0f;
    KD_TREE_NODE *son_ptr = root->left_son_ptr;
    if (son_ptr == nullptr)
        son_ptr = root->right_son_ptr;
    delete_evaluation = float(root->invalid_point_num) / root->TreeSize;
    balance_evaluation = float(son_ptr->TreeSize) / (root->TreeSize - 1);
    if (delete_evaluation > delete_criterion_param)
    {
        return true;
    }
    if (balance_evaluation > balance_criterion_param || balance_evaluation < 1 - balance_criterion_param)
    {
        return true;
    }
    return false;
}

template <typename PointType>
void KD_TREE<PointType>::Push_Down(KD_TREE_NODE *root)
{
    if (root == nullptr)
        return;
    Operation_Logger_Type operation;
    operation.op = PUSH_DOWN;
    operation.tree_deleted = root->tree_deleted;
    operation.tree_downsample_deleted = root->tree_downsample_deleted;
    if (root->need_push_down_to_left && root->left_son_ptr != nullptr)
    {
        if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != root->left_son_ptr)
        {
            root->left_son_ptr->tree_downsample_deleted |= root->tree_downsample_deleted;
            root->left_son_ptr->point_downsample_deleted |= root->tree_downsample_deleted;
            root->left_son_ptr->tree_deleted = root->tree_deleted || root->left_son_ptr->tree_downsample_deleted;
            root->left_son_ptr->point_deleted = root->left_son_ptr->tree_deleted || root->left_son_ptr->point_downsample_deleted;
            if (root->tree_downsample_deleted)
                root->left_son_ptr->down_del_num = root->left_son_ptr->TreeSize;
            if (root->tree_deleted)
                root->left_son_ptr->invalid_point_num = root->left_son_ptr->TreeSize;
            else
                root->left_son_ptr->invalid_point_num = root->left_son_ptr->down_del_num;
            root->left_son_ptr->need_push_down_to_left = true;
            root->left_son_ptr->need_push_down_to_right = true;
            root->need_push_down_to_left = false;
        }
        else
        {
            pthread_mutex_lock(&working_flag_mutex);
            root->left_son_ptr->tree_downsample_deleted |= root->tree_downsample_deleted;
            root->left_son_ptr->point_downsample_deleted |= root->tree_downsample_deleted;
            root->left_son_ptr->tree_deleted = root->tree_deleted || root->left_son_ptr->tree_downsample_deleted;
            root->left_son_ptr->point_deleted = root->left_son_ptr->tree_deleted || root->left_son_ptr->point_downsample_deleted;
            if (root->tree_downsample_deleted)
                root->left_son_ptr->down_del_num = root->left_son_ptr->TreeSize;
            if (root->tree_deleted)
                root->left_son_ptr->invalid_point_num = root->left_son_ptr->TreeSize;
            else
                root->left_son_ptr->invalid_point_num = root->left_son_ptr->down_del_num;
            root->left_son_ptr->need_push_down_to_left = true;
            root->left_son_ptr->need_push_down_to_right = true;
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(operation);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            root->need_push_down_to_left = false;
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    if (root->need_push_down_to_right && root->right_son_ptr != nullptr)
    {
        if (Rebuild_Ptr == nullptr || *Rebuild_Ptr != root->right_son_ptr)
        {
            root->right_son_ptr->tree_downsample_deleted |= root->tree_downsample_deleted;
            root->right_son_ptr->point_downsample_deleted |= root->tree_downsample_deleted;
            root->right_son_ptr->tree_deleted = root->tree_deleted || root->right_son_ptr->tree_downsample_deleted;
            root->right_son_ptr->point_deleted = root->right_son_ptr->tree_deleted || root->right_son_ptr->point_downsample_deleted;
            if (root->tree_downsample_deleted)
                root->right_son_ptr->down_del_num = root->right_son_ptr->TreeSize;
            if (root->tree_deleted)
                root->right_son_ptr->invalid_point_num = root->right_son_ptr->TreeSize;
            else
                root->right_son_ptr->invalid_point_num = root->right_son_ptr->down_del_num;
            root->right_son_ptr->need_push_down_to_left = true;
            root->right_son_ptr->need_push_down_to_right = true;
            root->need_push_down_to_right = false;
        }
        else
        {
            pthread_mutex_lock(&working_flag_mutex);
            root->right_son_ptr->tree_downsample_deleted |= root->tree_downsample_deleted;
            root->right_son_ptr->point_downsample_deleted |= root->tree_downsample_deleted;
            root->right_son_ptr->tree_deleted = root->tree_deleted || root->right_son_ptr->tree_downsample_deleted;
            root->right_son_ptr->point_deleted = root->right_son_ptr->tree_deleted || root->right_son_ptr->point_downsample_deleted;
            if (root->tree_downsample_deleted)
                root->right_son_ptr->down_del_num = root->right_son_ptr->TreeSize;
            if (root->tree_deleted)
                root->right_son_ptr->invalid_point_num = root->right_son_ptr->TreeSize;
            else
                root->right_son_ptr->invalid_point_num = root->right_son_ptr->down_del_num;
            root->right_son_ptr->need_push_down_to_left = true;
            root->right_son_ptr->need_push_down_to_right = true;
            if (rebuild_flag)
            {
                pthread_mutex_lock(&rebuild_logger_mutex_lock);
                Rebuild_Logger.push(operation);
                pthread_mutex_unlock(&rebuild_logger_mutex_lock);
            }
            root->need_push_down_to_right = false;
            pthread_mutex_unlock(&working_flag_mutex);
        }
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::Update(KD_TREE_NODE *root)
{
    KD_TREE_NODE *left_son_ptr = root->left_son_ptr;
    KD_TREE_NODE *right_son_ptr = root->right_son_ptr;
    float tmp_range_x[2] = {INFINITY, -INFINITY};
    float tmp_range_y[2] = {INFINITY, -INFINITY};
    float tmp_range_z[2] = {INFINITY, -INFINITY};
    // Update Tree Size
    if (left_son_ptr != nullptr && right_son_ptr != nullptr)
    {
        root->TreeSize = left_son_ptr->TreeSize + right_son_ptr->TreeSize + 1;
        root->invalid_point_num = left_son_ptr->invalid_point_num + right_son_ptr->invalid_point_num + (root->point_deleted ? 1 : 0);
        root->down_del_num = left_son_ptr->down_del_num + right_son_ptr->down_del_num + (root->point_downsample_deleted ? 1 : 0);
        root->tree_downsample_deleted = left_son_ptr->tree_downsample_deleted & right_son_ptr->tree_downsample_deleted & root->point_downsample_deleted;
        root->tree_deleted = left_son_ptr->tree_deleted && right_son_ptr->tree_deleted && root->point_deleted;
        if (root->tree_deleted || (!left_son_ptr->tree_deleted && !right_son_ptr->tree_deleted && !root->point_deleted))
        {
            tmp_range_x[0] = std::min(std::min(left_son_ptr->node_range_x[0], right_son_ptr->node_range_x[0]), root->point.x);
            tmp_range_x[1] = std::max(std::max(left_son_ptr->node_range_x[1], right_son_ptr->node_range_x[1]), root->point.x);
            tmp_range_y[0] = std::min(std::min(left_son_ptr->node_range_y[0], right_son_ptr->node_range_y[0]), root->point.y);
            tmp_range_y[1] = std::max(std::max(left_son_ptr->node_range_y[1], right_son_ptr->node_range_y[1]), root->point.y);
            tmp_range_z[0] = std::min(std::min(left_son_ptr->node_range_z[0], right_son_ptr->node_range_z[0]), root->point.z);
            tmp_range_z[1] = std::max(std::max(left_son_ptr->node_range_z[1], right_son_ptr->node_range_z[1]), root->point.z);
        }
        else
        {
            if (!left_son_ptr->tree_deleted)
            {
                tmp_range_x[0] = std::min(tmp_range_x[0], left_son_ptr->node_range_x[0]);
                tmp_range_x[1] = std::max(tmp_range_x[1], left_son_ptr->node_range_x[1]);
                tmp_range_y[0] = std::min(tmp_range_y[0], left_son_ptr->node_range_y[0]);
                tmp_range_y[1] = std::max(tmp_range_y[1], left_son_ptr->node_range_y[1]);
                tmp_range_z[0] = std::min(tmp_range_z[0], left_son_ptr->node_range_z[0]);
                tmp_range_z[1] = std::max(tmp_range_z[1], left_son_ptr->node_range_z[1]);
            }
            if (!right_son_ptr->tree_deleted)
            {
                tmp_range_x[0] = std::min(tmp_range_x[0], right_son_ptr->node_range_x[0]);
                tmp_range_x[1] = std::max(tmp_range_x[1], right_son_ptr->node_range_x[1]);
                tmp_range_y[0] = std::min(tmp_range_y[0], right_son_ptr->node_range_y[0]);
                tmp_range_y[1] = std::max(tmp_range_y[1], right_son_ptr->node_range_y[1]);
                tmp_range_z[0] = std::min(tmp_range_z[0], right_son_ptr->node_range_z[0]);
                tmp_range_z[1] = std::max(tmp_range_z[1], right_son_ptr->node_range_z[1]);
            }
            if (!root->point_deleted)
            {
                tmp_range_x[0] = std::min(tmp_range_x[0], root->point.x);
                tmp_range_x[1] = std::max(tmp_range_x[1], root->point.x);
                tmp_range_y[0] = std::min(tmp_range_y[0], root->point.y);
                tmp_range_y[1] = std::max(tmp_range_y[1], root->point.y);
                tmp_range_z[0] = std::min(tmp_range_z[0], root->point.z);
                tmp_range_z[1] = std::max(tmp_range_z[1], root->point.z);
            }
        }
    }
    else if (left_son_ptr != nullptr)
    {
        root->TreeSize = left_son_ptr->TreeSize + 1;
        root->invalid_point_num = left_son_ptr->invalid_point_num + (root->point_deleted ? 1 : 0);
        root->down_del_num = left_son_ptr->down_del_num + (root->point_downsample_deleted ? 1 : 0);
        root->tree_downsample_deleted = left_son_ptr->tree_downsample_deleted & root->point_downsample_deleted;
        root->tree_deleted = left_son_ptr->tree_deleted && root->point_deleted;
        if (root->tree_deleted || (!left_son_ptr->tree_deleted && !root->point_deleted))
        {
            tmp_range_x[0] = std::min(left_son_ptr->node_range_x[0], root->point.x);
            tmp_range_x[1] = std::max(left_son_ptr->node_range_x[1], root->point.x);
            tmp_range_y[0] = std::min(left_son_ptr->node_range_y[0], root->point.y);
            tmp_range_y[1] = std::max(left_son_ptr->node_range_y[1], root->point.y);
            tmp_range_z[0] = std::min(left_son_ptr->node_range_z[0], root->point.z);
            tmp_range_z[1] = std::max(left_son_ptr->node_range_z[1], root->point.z);
        }
        else
        {
            if (!left_son_ptr->tree_deleted)
            {
                tmp_range_x[0] = std::min(tmp_range_x[0], left_son_ptr->node_range_x[0]);
                tmp_range_x[1] = std::max(tmp_range_x[1], left_son_ptr->node_range_x[1]);
                tmp_range_y[0] = std::min(tmp_range_y[0], left_son_ptr->node_range_y[0]);
                tmp_range_y[1] = std::max(tmp_range_y[1], left_son_ptr->node_range_y[1]);
                tmp_range_z[0] = std::min(tmp_range_z[0], left_son_ptr->node_range_z[0]);
                tmp_range_z[1] = std::max(tmp_range_z[1], left_son_ptr->node_range_z[1]);
            }
            if (!root->point_deleted)
            {
                tmp_range_x[0] = std::min(tmp_range_x[0], root->point.x);
                tmp_range_x[1] = std::max(tmp_range_x[1], root->point.x);
                tmp_range_y[0] = std::min(tmp_range_y[0], root->point.y);
                tmp_range_y[1] = std::max(tmp_range_y[1], root->point.y);
                tmp_range_z[0] = std::min(tmp_range_z[0], root->point.z);
                tmp_range_z[1] = std::max(tmp_range_z[1], root->point.z);
            }
        }
    }
    else if (right_son_ptr != nullptr)
    {
        root->TreeSize = right_son_ptr->TreeSize + 1;
        root->invalid_point_num = right_son_ptr->invalid_point_num + (root->point_deleted ? 1 : 0);
        root->down_del_num = right_son_ptr->down_del_num + (root->point_downsample_deleted ? 1 : 0);
        root->tree_downsample_deleted = right_son_ptr->tree_downsample_deleted & root->point_downsample_deleted;
        root->tree_deleted = right_son_ptr->tree_deleted && root->point_deleted;
        if (root->tree_deleted || (!right_son_ptr->tree_deleted && !root->point_deleted))
        {
            tmp_range_x[0] = std::min(right_son_ptr->node_range_x[0], root->point.x);
            tmp_range_x[1] = std::max(right_son_ptr->node_range_x[1], root->point.x);
            tmp_range_y[0] = std::min(right_son_ptr->node_range_y[0], root->point.y);
            tmp_range_y[1] = std::max(right_son_ptr->node_range_y[1], root->point.y);
            tmp_range_z[0] = std::min(right_son_ptr->node_range_z[0], root->point.z);
            tmp_range_z[1] = std::max(right_son_ptr->node_range_z[1], root->point.z);
        }
        else
        {
            if (!right_son_ptr->tree_deleted)
            {
                tmp_range_x[0] = std::min(tmp_range_x[0], right_son_ptr->node_range_x[0]);
                tmp_range_x[1] = std::max(tmp_range_x[1], right_son_ptr->node_range_x[1]);
                tmp_range_y[0] = std::min(tmp_range_y[0], right_son_ptr->node_range_y[0]);
                tmp_range_y[1] = std::max(tmp_range_y[1], right_son_ptr->node_range_y[1]);
                tmp_range_z[0] = std::min(tmp_range_z[0], right_son_ptr->node_range_z[0]);
                tmp_range_z[1] = std::max(tmp_range_z[1], right_son_ptr->node_range_z[1]);
            }
            if (!root->point_deleted)
            {
                tmp_range_x[0] = std::min(tmp_range_x[0], root->point.x);
                tmp_range_x[1] = std::max(tmp_range_x[1], root->point.x);
                tmp_range_y[0] = std::min(tmp_range_y[0], root->point.y);
                tmp_range_y[1] = std::max(tmp_range_y[1], root->point.y);
                tmp_range_z[0] = std::min(tmp_range_z[0], root->point.z);
                tmp_range_z[1] = std::max(tmp_range_z[1], root->point.z);
            }
        }
    }
    else
    {
        root->TreeSize = 1;
        root->invalid_point_num = (root->point_deleted ? 1 : 0);
        root->down_del_num = (root->point_downsample_deleted ? 1 : 0);
        root->tree_downsample_deleted = root->point_downsample_deleted;
        root->tree_deleted = root->point_deleted;
        tmp_range_x[0] = root->point.x;
        tmp_range_x[1] = root->point.x;
        tmp_range_y[0] = root->point.y;
        tmp_range_y[1] = root->point.y;
        tmp_range_z[0] = root->point.z;
        tmp_range_z[1] = root->point.z;
    }
    memcpy(root->node_range_x, tmp_range_x, sizeof(tmp_range_x));
    memcpy(root->node_range_y, tmp_range_y, sizeof(tmp_range_y));
    memcpy(root->node_range_z, tmp_range_z, sizeof(tmp_range_z));
    float x_L = (root->node_range_x[1] - root->node_range_x[0]) * 0.5;
    float y_L = (root->node_range_y[1] - root->node_range_y[0]) * 0.5;
    float z_L = (root->node_range_z[1] - root->node_range_z[0]) * 0.5;
    root->radius_sq = x_L*x_L + y_L * y_L + z_L * z_L;
    if (left_son_ptr != nullptr)
        left_son_ptr->father_ptr = root;
    if (right_son_ptr != nullptr)
        right_son_ptr->father_ptr = root;
    if (root == Root_Node && root->TreeSize > 3)
    {
        KD_TREE_NODE *son_ptr = root->left_son_ptr;
        if (son_ptr == nullptr)
            son_ptr = root->right_son_ptr;
        float tmp_bal = float(son_ptr->TreeSize) / (root->TreeSize - 1);
        root->alpha_del = float(root->invalid_point_num) / root->TreeSize;
        root->alpha_bal = (tmp_bal >= 0.5 - EPSS) ? tmp_bal : 1 - tmp_bal;
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::flatten(KD_TREE_NODE *root, PointVector &Storage, delete_point_storage_set storage_type)
{
    if (root == nullptr)
        return;
    Push_Down(root);
    if (!root->point_deleted)
    {
        Storage.push_back(root->point);
    }
    flatten(root->left_son_ptr, Storage, storage_type);
    flatten(root->right_son_ptr, Storage, storage_type);
    switch (storage_type)
    {
        case NOT_RECORD:
            break;
        case DELETE_POINTS_REC:
            if (root->point_deleted && !root->point_downsample_deleted)
            {
                Points_deleted.push_back(root->point);
            }
            break;
        case MULTI_THREAD_REC:
            if (root->point_deleted && !root->point_downsample_deleted)
            {
                Multithread_Points_deleted.push_back(root->point);
            }
            break;
        default:
            break;
    }
    return;
}

template <typename PointType>
void KD_TREE<PointType>::delete_tree_nodes(KD_TREE_NODE **root)
{
    if (*root == nullptr)
        return;
    Push_Down(*root);
    delete_tree_nodes(&(*root)->left_son_ptr);
    delete_tree_nodes(&(*root)->right_son_ptr);

    pthread_mutex_destroy(&(*root)->push_down_mutex_lock);
    delete *root;
    *root = nullptr;

    return;
}

template <typename PointType>
bool KD_TREE<PointType>::same_point(const PointType &a, const PointType &b)
{
    return (std::fabs(a.x - b.x) < EPSS && std::fabs(a.y - b.y) < EPSS && std::fabs(a.z - b.z) < EPSS);
}

template <typename PointType>
float KD_TREE<PointType>::calc_dist(const PointType &a, const PointType &b)
{
    float dist = 0.0f;
    dist = (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z);
    return dist;
}

template <typename PointType>
float KD_TREE<PointType>::calc_box_dist(KD_TREE_NODE *node, const PointType &point)
{
    if (node == nullptr)
        return INFINITY;
    float min_dist = 0.0;
    if (point.x < node->node_range_x[0])
        min_dist += (point.x - node->node_range_x[0]) * (point.x - node->node_range_x[0]);
    if (point.x > node->node_range_x[1])
        min_dist += (point.x - node->node_range_x[1]) * (point.x - node->node_range_x[1]);
    if (point.y < node->node_range_y[0])
        min_dist += (point.y - node->node_range_y[0]) * (point.y - node->node_range_y[0]);
    if (point.y > node->node_range_y[1])
        min_dist += (point.y - node->node_range_y[1]) * (point.y - node->node_range_y[1]);
    if (point.z < node->node_range_z[0])
        min_dist += (point.z - node->node_range_z[0]) * (point.z - node->node_range_z[0]);
    if (point.z > node->node_range_z[1])
        min_dist += (point.z - node->node_range_z[1]) * (point.z - node->node_range_z[1]);
    return min_dist;
}
template <typename PointType>
bool KD_TREE<PointType>::point_cmp_x(const PointType &a, const PointType &b) { return a.x < b.x; }
template <typename PointType>
bool KD_TREE<PointType>::point_cmp_y(const PointType &a, const PointType &b) { return a.y < b.y; }
template <typename PointType>
bool KD_TREE<PointType>::point_cmp_z(const PointType &a, const PointType &b) { return a.z < b.z; }


// Manual Instatiations
template class KD_TREE<PointType_Coverage>;