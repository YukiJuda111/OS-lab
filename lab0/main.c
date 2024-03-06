#include <stdio.h>

int merge(int r[],int s[],int left,int mid,int right)
{
    int i,j,k;
    i=left;
    j=mid+1;
    k=left;
    while((i<=mid)&&(j<=right))
        if(r[i]<=r[j])
        {
            s[k] = r[i];
            i++;
            k++;
        }
        else
        {
            s[k]=r[j];
            j++;
            k++;
        }
    while(i<=mid)
        s[k++]=r[i++];
    while(j<=right)
        s[k++]=r[j++];
    return 0;
}

int merge_sort(int r[],int s[],int left,int right)
{
    int mid;
    int t[20];
    if(left==right)
        s[left]=r[right];
    else
    {
        mid=(left+right)/2;
        merge_sort(r,t,left,mid);
        merge_sort(r,t,mid+1,right);
        merge(t,s,left,mid,right);
    }
    return 0;
}

int main()
{
    int a[10];
    int i;

    for(i=0;i<10;i++)
        scanf("%d",&a[i]);
    merge_sort(a,a,0,9);

    for(i=0;i<10;i++)
        printf("%d ",a[i]);
    return 0;
}
